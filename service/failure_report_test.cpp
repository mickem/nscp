// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "failure_report.hpp"

#include <gtest/gtest.h>

#include <boost/json.hpp>
#include <string>

using failed_check_store = nsclient::core::failed_check_store;
using failure_report = nsclient::core::failure_report;
namespace json = boost::json;

namespace {
PB::Commands::QueryResponseMessage::Response result(const std::string &command, const PB::Common::ResultCode code, const std::string &message) {
  PB::Commands::QueryResponseMessage::Response response;
  response.set_command(command);
  response.set_result(code);
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();
  line->set_message(message);
  PB::Common::PerformanceData *perf = line->add_perf();
  perf->set_alias("load");
  perf->mutable_float_value()->set_value(97);
  perf->mutable_float_value()->set_unit("%");
  return response;
}

nsclient::core::metrics_snapshot_ptr snapshot(nsclient::core::metrics_snapshot_store &snapshots, const std::int64_t now) {
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *system = message.add_payload()->add_bundles();
  system->set_key("system");
  PB::Metrics::Metric *idle = system->add_value();
  idle->set_key("cpu.idle");
  idle->set_unit("percent");
  idle->mutable_gauge_value()->set_value(3);
  return snapshots.store(message, now);
}

failed_check_store::options capturing() {
  failed_check_store::options o;
  o.capture_metrics = true;
  return o;
}

json::object render(const failure_report &report, const std::int64_t now) { return json::parse(report.render(now)).as_object(); }
}  // namespace

TEST(failure_report, sends_the_newest_failure_with_a_snapshot_and_summarises_the_rest) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  store.configure(capturing());
  ASSERT_TRUE(store.record(result("check_cpu", PB::Common::ResultCode::CRITICAL, "cpu high"), nullptr, "Scheduler", 0, 1000, snapshot(snapshots, 995)));
  ASSERT_TRUE(store.record(result("check_cpu", PB::Common::ResultCode::CRITICAL, "cpu high"), nullptr, "Scheduler", 0, 1010));
  ASSERT_TRUE(store.record(result("check_memory", PB::Common::ResultCode::WARNING, "mem"), nullptr, "NRPEServer", 0, 1020));

  failure_report report = failure_report::from(store, store.list());
  report.since = 900;
  report.evicted = 2;
  const json::object root = render(report, 1100);

  EXPECT_EQ("1970-01-01T00:18:20Z", root.at("sent_at").as_string());
  // Entry 1 is the only one with a snapshot, so it is the one sent, although
  // two newer failures exist.
  const json::object &failure = root.at("failure").as_object();
  EXPECT_EQ(1, failure.at("index").as_int64());
  EXPECT_EQ("check_cpu", failure.at("command").as_string());
  EXPECT_EQ(2, failure.at("status").as_int64());
  EXPECT_EQ("CRITICAL", failure.at("result").as_string());
  EXPECT_EQ("cpu high", failure.at("message").as_string());
  EXPECT_EQ("Scheduler", failure.at("subject").as_string());
  EXPECT_EQ("1970-01-01T00:16:40Z", failure.at("date").as_string());
  EXPECT_EQ(97, failure.at("lines").as_array().at(0).as_object().at("perf").as_object().at("load").as_object().at("value").to_number<double>());
  // Not recorded, so not sent; and the context is reported on its own.
  EXPECT_EQ(nullptr, failure.if_contains("arguments"));
  EXPECT_EQ(nullptr, failure.if_contains("context"));

  const json::object &context = root.at("context").as_object();
  EXPECT_EQ("captured", context.at("state").as_string());
  EXPECT_EQ(5, context.at("age_seconds").as_int64());
  EXPECT_EQ("1970-01-01T00:16:35Z", context.at("captured_at").as_string());
  EXPECT_EQ(3, context.at("metrics").as_object().at("system.cpu.idle").as_int64());
  EXPECT_EQ("percent", context.at("metadata").as_object().at("system.cpu.idle").as_object().at("unit").as_string());

  const json::object &suppressed = root.at("suppressed").as_object();
  EXPECT_EQ("1970-01-01T00:15:00Z", suppressed.at("since").as_string());
  EXPECT_EQ(2, suppressed.at("count").as_int64());
  EXPECT_EQ(2, suppressed.at("evicted").as_int64());
  const json::array &by_check = suppressed.at("by_check").as_array();
  ASSERT_EQ(2u, by_check.size());
  // Newest first: check_memory (1020) leads, then check_cpu (1010).
  EXPECT_EQ("check_memory", by_check.at(0).as_object().at("command").as_string());
  EXPECT_EQ(1, by_check.at(0).as_object().at("status").as_int64());
  EXPECT_EQ("check_cpu", by_check.at(1).as_object().at("command").as_string());
  EXPECT_EQ(1, by_check.at(1).as_object().at("count").as_int64());
}

TEST(failure_report, groups_repeats_of_one_check_and_status) {
  failed_check_store store;
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(store.record(result("check_cpu", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 1000 + i));
  }
  ASSERT_TRUE(store.record(result("check_cpu", PB::Common::ResultCode::WARNING, "x"), nullptr, "s", 0, 1010));
  const failure_report report = failure_report::from(store, store.list());
  // Nothing has a snapshot: the newest is sent.
  EXPECT_EQ(5u, report.failure.index());
  const json::array by_check = render(report, 2000).at("suppressed").as_object().at("by_check").as_array();
  ASSERT_EQ(1u, by_check.size());
  const json::object &group = by_check.at(0).as_object();
  EXPECT_EQ(4, group.at("count").as_int64());
  EXPECT_EQ(2, group.at("status").as_int64());
  EXPECT_EQ("1970-01-01T00:16:40Z", group.at("first").as_string());
  EXPECT_EQ("1970-01-01T00:16:43Z", group.at("last").as_string());
}

TEST(failure_report, says_disabled_and_carries_no_metrics_when_capture_is_off) {
  failed_check_store store;
  ASSERT_TRUE(store.record(result("check_cpu", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 1000));
  const json::object root = render(failure_report::from(store, store.list()), 1100);
  const json::object &context = root.at("context").as_object();
  EXPECT_EQ("disabled", context.at("state").as_string());
  EXPECT_EQ(nullptr, context.if_contains("metrics"));
  EXPECT_EQ(0, root.at("suppressed").as_object().at("count").as_int64());
}

TEST(failure_report, a_snapshot_released_before_it_was_read_reports_as_evicted) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  store.configure(capturing());
  ASSERT_TRUE(store.record(result("check_cpu", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 1000, snapshot(snapshots, 995)));
  failure_report report = failure_report::from(store, store.list());
  report.context.reset();
  const json::object root = render(report, 1100);
  const json::object &context = root.at("context").as_object();
  EXPECT_EQ("evicted", context.at("state").as_string());
  EXPECT_EQ(nullptr, context.if_contains("metrics"));
}

TEST(failure_report, sends_arguments_only_when_they_are_recorded) {
  failed_check_store store;
  failed_check_store::options o;
  o.record_arguments = true;
  store.configure(o);
  google::protobuf::RepeatedPtrField<std::string> arguments;
  arguments.Add()->assign("password=hunter2");
  ASSERT_TRUE(store.record(result("check_sql", PB::Common::ResultCode::CRITICAL, "x"), &arguments, "s", 0, 1000));
  const json::object root = render(failure_report::from(store, store.list()), 1100);
  const json::array &sent = root.at("failure").as_object().at("arguments").as_array();
  ASSERT_EQ(1u, sent.size());
  // The store's redaction is what leaves the host.
  EXPECT_EQ("password=***", sent.at(0).as_string());
}
