// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "metrics_prune.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <metrics/metrics_json.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <string>

#include "failed_check_store.hpp"

using metrics_prune = nsclient::core::metrics_prune;
using nscapi::metrics::for_instance;
using nscapi::metrics::instance_scope;
using nscapi::metrics::metric;
namespace json = boost::json;

namespace {
void disk(PB::Metrics::MetricsBundle *section, const std::string &name, const double reads, const double idle) {
  const instance_scope d = for_instance(section, name, "disk");
  d.metric("reads_per_sec").gauge(reads);
  d.metric("percent_idle_time").gauge(idle);
  d.metric("queue_length").gauge(0);
}

void nic(PB::Metrics::MetricsBundle *section, const std::string &name, const std::string &mac) {
  const instance_scope n = for_instance(section, name, "nic");
  n.metric("BytesTotalPersec").gauge(0);
  n.metric("MACAddress").info(mac);
}

// The shape a Windows host publishes: `_Total` and the only busy disk are
// identical, the idle disk and an unmounted volume are too, and most
// adapters are idle with no address.
PB::Metrics::MetricsMessage snapshot() {
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *system = message.add_payload()->add_bundles();
  system->set_key("system");
  PB::Metrics::MetricsBundle *io = system->add_children();
  io->set_key("io");
  disk(io, "_Total", 859, 98);
  disk(io, "C:", 859, 98);
  disk(io, "D:", 0, 99);
  disk(io, "HarddiskVolume1", 0, 99);
  PB::Metrics::MetricsBundle *network = system->add_children();
  network->set_key("network");
  nic(network, "Ethernet", "BC:FC:E7:1C:A0:9C");
  nic(network, "WAN Miniport (IP)", "");
  nic(network, "WAN Miniport (IPv6)", "");
  metric(system, "errors").counter(0);
  metric(system, "uptime").info("3d 12:46");
  return message;
}

json::object flat(const PB::Metrics::MetricsMessage &message) {
  json::object out;
  metrics_json::build(out, nullptr, message);
  return out;
}

metrics_prune::rules all() {
  metrics_prune::rules r;
  r.zeros = r.empty = r.duplicates = true;
  return r;
}
}  // namespace

TEST(MetricsPrune, NoRulesLeavesTheSnapshotAlone) {
  PB::Metrics::MetricsMessage message = snapshot();
  const std::string before = message.SerializeAsString();
  const metrics_prune::summary s = metrics_prune::apply(message, metrics_prune::rules());
  EXPECT_FALSE(s.any());
  EXPECT_EQ(before, message.SerializeAsString());
}

TEST(MetricsPrune, ZerosDropsEveryNumericZeroOfAnyType) {
  PB::Metrics::MetricsMessage message = snapshot();
  metrics_prune::rules r;
  r.zeros = true;
  const metrics_prune::summary s = metrics_prune::apply(message, r);
  const json::object out = flat(message);
  EXPECT_FALSE(out.contains("system.errors"));  // a counter
  EXPECT_FALSE(out.contains("system.io.C:.queue_length"));
  EXPECT_FALSE(out.contains("system.io.D:.reads_per_sec"));
  EXPECT_TRUE(out.contains("system.io.D:.percent_idle_time"));
  // An empty string is not a zero.
  EXPECT_TRUE(out.contains("system.network.WAN Miniport (IP).MACAddress"));
  // 4 queue lengths, 2 idle disks' reads, 3 adapters' bytes, errors.
  EXPECT_EQ(10u, s.zeros);
  EXPECT_EQ(0u, s.empty);
  EXPECT_TRUE(s.collapsed.empty());
}

TEST(MetricsPrune, EmptyDropsEmptyStringsOnly) {
  PB::Metrics::MetricsMessage message = snapshot();
  metrics_prune::rules r;
  r.empty = true;
  const metrics_prune::summary s = metrics_prune::apply(message, r);
  const json::object out = flat(message);
  EXPECT_FALSE(out.contains("system.network.WAN Miniport (IP).MACAddress"));
  EXPECT_EQ("BC:FC:E7:1C:A0:9C", out.at("system.network.Ethernet.MACAddress").as_string());
  EXPECT_TRUE(out.contains("system.network.WAN Miniport (IP).BytesTotalPersec"));
  EXPECT_EQ(2u, s.empty);
}

TEST(MetricsPrune, DuplicatesKeepsTheFirstInstanceAndNamesTheRest) {
  PB::Metrics::MetricsMessage message = snapshot();
  metrics_prune::rules r;
  r.duplicates = true;
  const metrics_prune::summary s = metrics_prune::apply(message, r);
  const json::object out = flat(message);
  EXPECT_TRUE(out.contains("system.io._Total.reads_per_sec"));
  EXPECT_FALSE(out.contains("system.io.C:.reads_per_sec"));
  EXPECT_TRUE(out.contains("system.io.D:.reads_per_sec"));
  EXPECT_FALSE(out.contains("system.io.HarddiskVolume1.percent_idle_time"));
  EXPECT_TRUE(out.contains("system.network.WAN Miniport (IP).BytesTotalPersec"));
  EXPECT_FALSE(out.contains("system.network.WAN Miniport (IPv6).BytesTotalPersec"));
  // Not instances, so never collapsed.
  EXPECT_TRUE(out.contains("system.errors"));

  ASSERT_EQ(3u, s.collapsed.size());
  EXPECT_EQ("system.io.C:", s.collapsed[0].first);
  EXPECT_EQ("system.io._Total", s.collapsed[0].second);
  EXPECT_EQ("system.io.HarddiskVolume1", s.collapsed[1].first);
  EXPECT_EQ("system.io.D:", s.collapsed[1].second);
  EXPECT_EQ("system.network.WAN Miniport (IPv6)", s.collapsed[2].first);
  EXPECT_EQ("system.network.WAN Miniport (IP)", s.collapsed[2].second);
  EXPECT_EQ(3u + 3u + 2u, s.duplicates);
}

TEST(MetricsPrune, DuplicatesCompareTheValuesAsCollectedNotAfterTrimming) {
  // D: and E: only differ in a value the zeros rule removes: they are not
  // duplicates, and both are kept.
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *io = message.add_payload()->add_bundles();
  io->set_key("io");
  disk(io, "D:", 0, 99);
  disk(io, "E:", 1, 99);
  const metrics_prune::summary s = metrics_prune::apply(message, all());
  EXPECT_TRUE(s.collapsed.empty());
  const json::object out = flat(message);
  EXPECT_TRUE(out.contains("io.D:.percent_idle_time"));
  EXPECT_TRUE(out.contains("io.E:.percent_idle_time"));
}

TEST(MetricsPrune, DuplicatesNeedEveryValueEqualToTheLastDigit) {
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *io = message.add_payload()->add_bundles();
  io->set_key("io");
  const instance_scope a = for_instance(io, "a", "disk");
  a.metric("latency").gauge(0.17653707224334603);
  const instance_scope b = for_instance(io, "b", "disk");
  b.metric("latency").gauge(std::nextafter(0.17653707224334603, 1.0));
  EXPECT_TRUE(metrics_prune::apply(message, all()).collapsed.empty());
}

TEST(MetricsPrune, DuplicatesIgnoreTheOrderValuesWerePublishedIn) {
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *io = message.add_payload()->add_bundles();
  io->set_key("io");
  const instance_scope a = for_instance(io, "a", "disk");
  a.metric("x").gauge(1);
  a.metric("y").gauge(2);
  const instance_scope b = for_instance(io, "b", "disk");
  b.metric("y").gauge(2);
  b.metric("x").gauge(1);
  const metrics_prune::summary s = metrics_prune::apply(message, all());
  ASSERT_EQ(1u, s.collapsed.size());
  EXPECT_EQ("io.b", s.collapsed[0].first);
}

TEST(MetricsPrune, PatternsExcludeEveryMetricWhoseKeyContainsOne) {
  PB::Metrics::MetricsMessage message = snapshot();
  metrics_prune::rules r;
  r.patterns = metrics_prune::parse_patterns(" harddiskvolume1 , WAN MINIPORT,, ");
  ASSERT_EQ(2u, r.patterns.size());
  EXPECT_EQ("wan miniport", r.patterns[1]);
  const metrics_prune::summary s = metrics_prune::apply(message, r);
  const json::object out = flat(message);
  EXPECT_FALSE(out.contains("system.io.HarddiskVolume1.reads_per_sec"));
  EXPECT_FALSE(out.contains("system.network.WAN Miniport (IP).MACAddress"));
  EXPECT_FALSE(out.contains("system.network.WAN Miniport (IPv6).BytesTotalPersec"));
  EXPECT_TRUE(out.contains("system.network.Ethernet.MACAddress"));
  EXPECT_TRUE(out.contains("system.io.D:.reads_per_sec"));
  EXPECT_EQ(3u + 2u + 2u, s.excluded);
  EXPECT_TRUE(s.any());
}

TEST(MetricsPrune, PatternsMatchTheWholeKeyNotOnlyTheInstance) {
  PB::Metrics::MetricsMessage message = snapshot();
  metrics_prune::rules r;
  r.patterns = metrics_prune::parse_patterns(".MACAddress,system.errors");
  metrics_prune::apply(message, r);
  const json::object out = flat(message);
  EXPECT_FALSE(out.contains("system.network.Ethernet.MACAddress"));
  EXPECT_TRUE(out.contains("system.network.Ethernet.BytesTotalPersec"));
  EXPECT_FALSE(out.contains("system.errors"));
  EXPECT_TRUE(out.contains("system.uptime"));
}

TEST(MetricsPrune, AnExcludedInstanceIsNeverWhatADuplicateCollapsesInto) {
  // `_Total` is excluded, so C: - identical to it - has nothing to be a
  // duplicate of and stays.
  PB::Metrics::MetricsMessage message = snapshot();
  metrics_prune::rules r = all();
  r.patterns = metrics_prune::parse_patterns("_total");
  const metrics_prune::summary s = metrics_prune::apply(message, r);
  const json::object out = flat(message);
  EXPECT_FALSE(out.contains("system.io._Total.reads_per_sec"));
  EXPECT_TRUE(out.contains("system.io.C:.reads_per_sec"));
  for (const auto &c : s.collapsed) EXPECT_NE("system.io._Total", c.second);
}

TEST(MetricsPrune, PatternsCompareOnlyAsciiCaseInsensitively) {
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *network = message.add_payload()->add_bundles();
  network->set_key("network");
  nic(network, "Anslutning till lokalt n\xC3\xA4tverk* 1", "");
  metrics_prune::rules r;
  r.patterns = metrics_prune::parse_patterns("LOKALT N\xC3\xA4TVERK");
  metrics_prune::apply(message, r);
  EXPECT_TRUE(flat(message).empty());
}

TEST(MetricsPrune, ParseReadsAListAndRefusesAnUnknownRule) {
  metrics_prune::rules r;
  std::string error;
  ASSERT_TRUE(metrics_prune::parse(" Zeros, duplicates ", r, error));
  EXPECT_TRUE(r.zeros);
  EXPECT_FALSE(r.empty);
  EXPECT_TRUE(r.duplicates);
  EXPECT_EQ("zeros,duplicates", metrics_prune::format(r));

  r.patterns.push_back("vbox");
  ASSERT_TRUE(metrics_prune::parse("", r, error));
  EXPECT_FALSE(r.zeros || r.empty || r.duplicates);
  // The rules list does not touch the patterns.
  ASSERT_EQ(1u, r.patterns.size());
  r.patterns.clear();
  EXPECT_FALSE(r.any());
  ASSERT_TRUE(metrics_prune::parse("none", r, error));
  EXPECT_FALSE(r.any());

  r.zeros = true;
  EXPECT_FALSE(metrics_prune::parse("zeros,nulls", r, error));
  EXPECT_NE(std::string::npos, error.find("nulls"));
  // A refused list leaves the rules as they were.
  EXPECT_TRUE(r.zeros);
}

TEST(MetricsPrune, TheLogRecordsWhatWasPrunedWithTheEntry) {
  nsclient::core::metrics_snapshot_store snapshots;
  const nsclient::core::metrics_snapshot_ptr kept = snapshots.store(snapshot(), 1000, all());
  EXPECT_LT(kept->wire_size, snapshot().ByteSizeLong());

  nsclient::core::failed_check_store store;
  nsclient::core::failed_check_store::options options;
  options.capture_metrics = true;
  options.prune = all();
  store.configure(options);
  PB::Commands::QueryResponseMessage::Response response;
  response.set_command("check_cpu");
  response.set_result(PB::Common::ResultCode::CRITICAL);
  store.record(response, nullptr, "test", 0, 1001, kept);

  const nsclient::core::failed_check_store::entry_list entries = store.list();
  ASSERT_EQ(1u, entries.size());
  ASSERT_TRUE(entries[0].context().has_pruned());
  const PB::Failures::MetricsContext::Pruned &p = entries[0].context().pruned();
  EXPECT_EQ("zeros,empty,duplicates", p.rules());
  EXPECT_EQ(0, p.patterns_size());
  EXPECT_EQ(0u, p.excluded());
  ASSERT_EQ(3, p.collapsed_size());
  EXPECT_EQ("system.io.C:", p.collapsed(0).key());
  EXPECT_EQ("system.io._Total", p.collapsed(0).value());
}
