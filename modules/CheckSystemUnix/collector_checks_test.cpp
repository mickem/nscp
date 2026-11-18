// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_cpu, check_memory, check_pagefile and check_network: the four checks
// that render the background collector's buffers. Their keyword registries and
// unit converters are driven over fabricated records the way the command
// bodies drive them, and the command bodies themselves over a missing, a cold
// and a warmed-up collector.

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <string>
#include <thread>
#include <vector>

#include "check_cpu.h"
#include "check_memory.h"
#include "check_network.h"
#include "check_pagefile.h"
#include "realtime_thread.hpp"

namespace {

typedef PB::Commands::QueryResponseMessage::Response response_type;

PB::Commands::QueryRequestMessage::Request make_request(const std::string &command, const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

std::string message_of(const response_type &response) {
  std::string out;
  for (int i = 0; i < response.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += response.lines(i).message();
  }
  return out;
}

// The perf value with the given alias, or -1 when there is none.
double perf_value(const response_type &response, const std::string &alias) {
  for (int i = 0; i < response.lines_size(); ++i) {
    for (const auto &p : response.lines(i).perf()) {
      if (p.alias() == alias && p.has_float_value()) return p.float_value().value();
    }
  }
  return -1;
}

bool has_perf(const response_type &response, const std::string &alias) {
  for (int i = 0; i < response.lines_size(); ++i) {
    for (const auto &p : response.lines(i).perf()) {
      if (p.alias() == alias) return true;
    }
  }
  return false;
}

// Run the filter of check type F over the given records, set up with the same
// defaults as its command, and return the response.
template <class F, class R>
response_type run_filter(const std::string &command, const std::vector<std::string> &args, const std::vector<std::shared_ptr<R> > &records,
                         const std::string &warn, const std::string &crit, const std::string &top, const std::string &detail, const std::string &perf) {
  const auto request = make_request(command, args);
  response_type response;
  modern_filter::data_container data;
  modern_filter::cli_helper<F> helper(request, &response, data);
  F filter;
  helper.add_options(warn, crit, "", filter.get_filter_syntax(), "ignored");
  helper.add_syntax(top, detail, perf, "", "");
  if (!helper.parse_options()) return response;
  if (!helper.build_filter(filter)) return response;
  for (const auto &r : records) filter.match(r);
  helper.post_process(filter);
  return response;
}

const long long GiB = 1024LL * 1024 * 1024;

}  // namespace

// --- check_cpu -------------------------------------------------------------

namespace {
using cpu_obj = checks::check_cpu_filter::filter_obj;

response_type run_cpu(const std::vector<std::string> &args, const std::vector<std::shared_ptr<cpu_obj> > &records) {
  return run_filter<checks::check_cpu_filter::filter>("check_cpu", args, records, "load > 80", "load > 90", "${status}: ${problem_list}", "${time}: ${load}%",
                                                      "${core} ${time}");
}

std::vector<std::shared_ptr<cpu_obj> > cpu_records(double user, double kernel) {
  return {std::make_shared<cpu_obj>("5m", "total", user, kernel, 100 - user - kernel), std::make_shared<cpu_obj>("5m", "core 3", 1.0, 1.0, 98.0)};
}
}  // namespace

TEST(CheckCpuFilter, ObjectGetters) {
  const cpu_obj total("1m", "total", 12.6, 7.7, 79.7);
  EXPECT_EQ(20, total.get_total());
  EXPECT_EQ(12, total.get_user());
  EXPECT_EQ(7, total.get_kernel());
  EXPECT_EQ(79, total.get_idle());
  EXPECT_EQ(-1, total.get_core_i());
  EXPECT_EQ("total", total.get_core_id());
  EXPECT_EQ("core: total time: 1m load: 20%", total.show());

  const cpu_obj core("1m", "core 12", 0, 0, 100);
  EXPECT_EQ(12, core.get_core_i());
  EXPECT_EQ("core_12", core.get_core_id());
  EXPECT_EQ("core 12", core.get_core_s());

  // A core name that is not "core N" maps to 0 rather than throwing.
  EXPECT_EQ(0, cpu_obj("1m", "core x", 0, 0, 0).get_core_i());
  EXPECT_EQ(0, cpu_obj("1m", "cpu", 0, 0, 0).get_core_i());
}

TEST(CheckCpuFilter, DefaultThresholdsOnTheTotal) {
  EXPECT_EQ(PB::Common::ResultCode::OK, run_cpu({"filter=core = 'total'"}, cpu_records(30, 10)).result());
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_cpu({"filter=core = 'total'"}, cpu_records(70, 15)).result());
  const response_type crit = run_cpu({"filter=core = 'total'"}, cpu_records(80, 15));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, crit.result());
  EXPECT_EQ("CRITICAL: 5m: 95%", message_of(crit));
}

TEST(CheckCpuFilter, PercentLiteralGoesThroughTheConverter) {
  // load/usage/total/user are custom-pct keywords: a "%" literal goes through
  // the converter, which rounds it to a whole percent (49.4% -> 49, 50.4% -> 50).
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_cpu({"filter=core = 'total'", "warning=none", "critical=usage > 49.4%"}, cpu_records(40, 10)).result());
  EXPECT_EQ(PB::Common::ResultCode::OK, run_cpu({"filter=core = 'total'", "warning=none", "critical=usage > 50.4%"}, cpu_records(40, 10)).result());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_cpu({"filter=core = 'total'", "warning=none", "critical=user >= 40%"}, cpu_records(40, 10)).result());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_cpu({"filter=core = 'total'", "warning=none", "critical=total > 49%"}, cpu_records(40, 10)).result());
}

TEST(CheckCpuFilter, CoreKeywordsMatchByNameAndNumber) {
  const response_type by_name = run_cpu({"filter=core = 'core 3'", "warning=none", "critical=system > 0"}, cpu_records(90, 9));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, by_name.result()) << message_of(by_name);
  const response_type by_id =
      run_cpu({"filter=core_id = 'core_3'", "warning=none", "critical=idle < 99", "detail-syntax=${core_id}/${kernel}/${idle}"}, cpu_records(90, 9));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, by_id.result());
  EXPECT_EQ("CRITICAL: core_3/1/98", message_of(by_id));
  const response_type by_number = run_cpu({"filter=core > 2", "warning=none", "critical=load > 0"}, cpu_records(90, 9));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, by_number.result()) << message_of(by_number);
}

TEST(CheckCpuFilter, PerfDataPerKeyword) {
  const response_type r = run_cpu({"filter=core = 'total'", "warning=load > 80", "critical=load > 90"}, cpu_records(30, 10));
  ASSERT_EQ(PB::Common::ResultCode::OK, r.result());
  EXPECT_DOUBLE_EQ(40, perf_value(r, "total 5m"));
}

TEST(CheckCpuCommand, MissingCollectorIsUnknown) {
  response_type response;
  checks::check_cpu(nullptr, make_request("check_cpu", {}), &response);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_EQ("CPU collector not initialized", message_of(response));
}

TEST(CheckCpuCommand, BadTimeWindowIsRejectedBeforeTheCollectorIsRead) {
  response_type response;
  checks::check_cpu(nullptr, make_request("check_cpu", {"time=forever"}), &response);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_NE(std::string::npos, message_of(response).find("forever")) << message_of(response);
}

TEST(CheckCpuCommand, ColdCollectorAnswersWithTheWarmupState) {
  auto collector = std::make_shared<pdh_thread>();
  response_type response;
  checks::check_cpu(collector, make_request("check_cpu", {"warmup-state=ok"}), &response);
  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("No CPU data available yet (collector still initializing)", message_of(response));

  response_type unknown;
  checks::check_cpu(collector, make_request("check_cpu", {}), &unknown);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, unknown.result());
}

// --- check_memory ----------------------------------------------------------

namespace {
using mem_obj = check_memory::check_mem_filter::filter_obj;

response_type run_mem(const std::vector<std::string> &args, const std::vector<std::shared_ptr<mem_obj> > &records) {
  return run_filter<check_memory::check_mem_filter::filter>("check_memory", args, records, "used > 80%", "used > 90%", "${status}: ${list}",
                                                            "${type} = ${used}", "${type}");
}

std::vector<std::shared_ptr<mem_obj> > physical(long long free, long long total) { return {std::make_shared<mem_obj>("physical", free, total)}; }
}  // namespace

TEST(CheckMemoryFilter, ObjectGetters) {
  const mem_obj m("physical", 3 * GiB, 4 * GiB);
  EXPECT_EQ(4 * GiB, m.get_total());
  EXPECT_EQ(3 * GiB, m.get_free());
  EXPECT_EQ(1 * GiB, m.get_used());
  EXPECT_EQ("physical", m.get_type());
  EXPECT_EQ("type: physical total: 4GB free: 3GB used: 1GB", m.show());
  const mem_obj copy(m);
  EXPECT_EQ("physical", copy.get_type());
  EXPECT_EQ(4 * GiB, copy.get_total());
}

TEST(CheckMemoryFilter, PercentThresholds) {
  EXPECT_EQ(PB::Common::ResultCode::OK, run_mem({}, physical(2 * GiB, 4 * GiB)).result());
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_mem({}, physical(GiB / 2, 4 * GiB)).result());
  const response_type crit = run_mem({}, physical(GiB / 4, 4 * GiB));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, crit.result());
  EXPECT_EQ("CRITICAL: physical = 3.75GB", message_of(crit));
}

TEST(CheckMemoryFilter, ByteUnitThresholds) {
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_mem({"warning=none", "critical=free < 1G"}, physical(GiB / 2, 4 * GiB)).result());
  EXPECT_EQ(PB::Common::ResultCode::OK, run_mem({"warning=none", "critical=free < 256M"}, physical(GiB / 2, 4 * GiB)).result());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_mem({"warning=none", "critical=size >= 4294967296"}, physical(GiB / 2, 4 * GiB)).result());
}

TEST(CheckMemoryFilter, AHugePercentageSaturatesInsteadOfOverflowing) {
  // total * 1e17 overflows a long long; the converter computes in double and
  // saturates, so the comparison still comes out the sane way round.
  EXPECT_EQ(PB::Common::ResultCode::OK, run_mem({"warning=none", "critical=used > 100000000000000000%"}, physical(0, 4 * GiB)).result());
}

TEST(CheckMemoryFilter, HumanStringsAndPerfData) {
  const response_type r = run_mem({"warning=none", "critical=none", "detail-syntax=${type}: ${used}/${size} free ${free}"}, physical(GiB, 4 * GiB));
  ASSERT_EQ(PB::Common::ResultCode::OK, r.result());
  EXPECT_EQ("OK: physical: 3GB/4GB free 1GB", message_of(r));

  const response_type pct = run_mem({}, physical(GiB, 4 * GiB));
  EXPECT_TRUE(has_perf(pct, "physical")) << pct.DebugString();
  EXPECT_TRUE(has_perf(pct, "physical %")) << pct.DebugString();
  EXPECT_DOUBLE_EQ(75, perf_value(pct, "physical %"));
}

TEST(CheckMemoryCommand, MissingCollectorIsUnknown) {
  response_type response;
  check_memory::check_memory(nullptr, make_request("check_memory", {}), &response);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_EQ("Memory collector not initialized", message_of(response));
}

TEST(CheckMemoryCommand, ColdCollectorAnswersWithTheWarmupState) {
  auto collector = std::make_shared<pdh_thread>();
  response_type response;
  check_memory::check_memory(collector, make_request("check_memory", {"warmup-state=warning"}), &response);
  EXPECT_EQ(PB::Common::ResultCode::WARNING, response.result());
  EXPECT_EQ("No memory data available yet (collector still initializing)", message_of(response));
}

// --- check_pagefile --------------------------------------------------------

namespace {
using page_obj = check_page::check_page_filter::filter_obj;

response_type run_page(const std::vector<std::string> &args, const std::vector<std::shared_ptr<page_obj> > &records) {
  return run_filter<check_page::check_page_filter::filter>("check_pagefile", args, records, "used > 60%", "used > 80%", "${status}: ${list}",
                                                           "${name} ${used} (${size})", "${name}");
}

std::vector<std::shared_ptr<page_obj> > swap(long long total, long long used) { return {std::make_shared<page_obj>("total", total, used)}; }
}  // namespace

TEST(CheckPagefileFilter, ObjectGetters) {
  const page_obj p("total", 4 * GiB, GiB);
  EXPECT_EQ("total", p.show());
  EXPECT_EQ("total", p.get_name());
  EXPECT_EQ(4 * GiB, p.get_total());
  EXPECT_EQ(GiB, p.get_used());
  EXPECT_EQ(3 * GiB, p.get_free());
  EXPECT_EQ(25, p.get_used_pct());
  EXPECT_EQ(75, p.get_free_pct());
  // used > total (a racy read of the two counters) clamps free to zero.
  EXPECT_EQ(0, page_obj("total", GiB, 2 * GiB).get_free());
  const page_obj copy(p);
  EXPECT_EQ(GiB, copy.get_used());
}

TEST(CheckPagefileFilter, DefaultThresholds) {
  EXPECT_EQ(PB::Common::ResultCode::OK, run_page({}, swap(4 * GiB, GiB)).result());
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_page({}, swap(4 * GiB, 3 * GiB)).result());
  const response_type crit = run_page({}, swap(4 * GiB, 4 * GiB - GiB / 4));
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, crit.result());
  EXPECT_EQ("CRITICAL: total 3.75GB (4GB)", message_of(crit));
}

TEST(CheckPagefileFilter, ByteAndNameKeywords) {
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_page({"warning=none", "critical=free < 2G"}, swap(4 * GiB, 3 * GiB)).result());
  EXPECT_EQ(PB::Common::ResultCode::OK, run_page({"warning=none", "critical=free < 512M"}, swap(4 * GiB, 3 * GiB)).result());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_page({"filter=name = 'total'", "warning=none", "critical=used > 0"}, swap(4 * GiB, 3 * GiB)).result());
  // Filtering the only record out leaves nothing: the empty-state answer.
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, run_page({"filter=name = 'other'", "warning=none", "critical=used > 0"}, swap(4 * GiB, 3 * GiB)).result());
  const response_type r = run_page({"warning=none", "critical=none", "detail-syntax=${name}: ${free} free"}, swap(4 * GiB, 3 * GiB));
  EXPECT_EQ("OK: total: 1GB free", message_of(r));
}

TEST(CheckPagefileCommand, MissingCollectorIsUnknown) {
  response_type response;
  check_page::check_pagefile(nullptr, make_request("check_pagefile", {}), &response);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_EQ("Pagefile collector not initialized", message_of(response));
}

TEST(CheckPagefileCommand, ColdCollectorAnswersWithTheWarmupState) {
  auto collector = std::make_shared<pdh_thread>();
  response_type response;
  check_page::check_pagefile(collector, make_request("check_pagefile", {"warmup-state=critical"}), &response);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, response.result());
  EXPECT_EQ("No pagefile/swap data available yet (collector still initializing)", message_of(response));
}

// --- check_network ---------------------------------------------------------

TEST(CheckNetworkCommand, MissingCollectorIsUnknown) {
  response_type response;
  network_check::check_network(nullptr, make_request("check_network", {}), &response);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_EQ("Network collector not initialized", message_of(response));
}

TEST(CheckNetworkCommand, ColdCollectorAnswersWithTheWarmupState) {
  auto collector = std::make_shared<pdh_thread>();
  response_type response;
  network_check::check_network(collector, make_request("check_network", {"warmup-state=ok"}), &response);
  EXPECT_EQ(PB::Common::ResultCode::OK, response.result());
  EXPECT_EQ("No network data available yet (collector still initializing)", message_of(response));
}

TEST(CheckNetworkCommand, BadFilterIsUnknown) {
  response_type response;
  network_check::check_network(nullptr, make_request("check_network", {"filter=no_such_keyword > 1"}), &response);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response.result());
  EXPECT_NE("Network collector not initialized", message_of(response));
}

TEST(CheckNetworkMetrics, OneSectionWithAnInstancePerInterface) {
  network_check::network_interface eth0;
  eth0.name = "eth0";
  eth0.status = "up";
  eth0.rx_bytes_per_sec = 1000;
  eth0.tx_bytes_per_sec = 500;
  network_check::network_interface lo;
  lo.name = "lo";
  lo.status = "unknown";

  PB::Metrics::MetricsBundle root;
  network_check::build_network_metrics(&root, {eth0, lo});
  ASSERT_EQ(1, root.children_size());
  const PB::Metrics::MetricsBundle &section = root.children(0);
  EXPECT_EQ("network", section.key());
  ASSERT_EQ(8, section.value_size());

  std::map<std::string, const PB::Metrics::Metric *> by_key;
  for (const auto &m : section.value()) by_key[m.key()] = &m;
  ASSERT_TRUE(by_key.count("eth0.received"));
  EXPECT_DOUBLE_EQ(1000, by_key["eth0.received"]->gauge_value().value());
  EXPECT_DOUBLE_EQ(500, by_key["eth0.sent"]->gauge_value().value());
  EXPECT_DOUBLE_EQ(1500, by_key["eth0.total"]->gauge_value().value());
  EXPECT_EQ("up", by_key["eth0.status"]->string_value().value());
  EXPECT_EQ("unknown", by_key["lo.status"]->string_value().value());
  // Rates carry no unit, and the interface rides a label.
  EXPECT_TRUE(by_key["eth0.received"]->unit().empty());
  ASSERT_EQ(1, by_key["eth0.received"]->dims_size());
  EXPECT_EQ("nic", by_key["eth0.received"]->dims(0).key());
  EXPECT_EQ("eth0", by_key["eth0.received"]->dims(0).value());
}

// --- against a live collector -----------------------------------------------

namespace {
// Start a collector and wait (bounded) until its CPU and memory buffers have
// data: the first tick takes a baseline, the second produces the first sample.
std::shared_ptr<pdh_thread> warmed_up_collector() {
  auto collector = std::make_shared<pdh_thread>();
  collector->start();
  for (int i = 0; i < 100; ++i) {
    if (collector->cpu_status().state == sampling::readiness::ready && collector->memory_status().state == sampling::readiness::ready &&
        collector->network_status().state != sampling::readiness::warming_up)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return collector;
}
}  // namespace

TEST(CollectorChecks, RenderLiveData) {
  const auto collector = warmed_up_collector();
  ASSERT_EQ(sampling::readiness::ready, collector->cpu_status().state);
  ASSERT_EQ(sampling::readiness::ready, collector->memory_status().state);

  // Thresholds pinned to never fire, so the state does not depend on the host.
  response_type cpu;
  checks::check_cpu(collector, make_request("check_cpu", {"time=1s", "warning=load > 1000", "critical=load > 1000", "cores=true"}), &cpu);
  EXPECT_EQ(PB::Common::ResultCode::OK, cpu.result()) << message_of(cpu);
  EXPECT_EQ("OK: CPU load is ok.", message_of(cpu));
  EXPECT_TRUE(has_perf(cpu, "total 1s")) << cpu.DebugString();

  response_type mem;
  check_memory::check_memory(collector, make_request("check_memory", {"warning=none", "critical=none", "type=physical"}), &mem);
  EXPECT_EQ(PB::Common::ResultCode::OK, mem.result()) << message_of(mem);
  EXPECT_EQ(0u, message_of(mem).find("OK: physical = ")) << message_of(mem);

  response_type bad_type;
  check_memory::check_memory(collector, make_request("check_memory", {"type=nonsense"}), &bad_type);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, bad_type.result());
  EXPECT_EQ("Invalid type: nonsense", message_of(bad_type));

  response_type page;
  check_page::check_pagefile(collector, make_request("check_pagefile", {"warning=none", "critical=none"}), &page);
  EXPECT_EQ(PB::Common::ResultCode::OK, page.result()) << message_of(page);
  EXPECT_EQ(0u, message_of(page).find("OK: total ")) << message_of(page);

  // A host without a single interface in /proc/net/dev is the documented
  // no-data contract (UNKNOWN), anything else renders.
  response_type net;
  network_check::check_network(collector, make_request("check_network", {"warning=none", "critical=none"}), &net);
  if (collector->network_status().state == sampling::readiness::ready) {
    EXPECT_EQ(PB::Common::ResultCode::OK, net.result()) << message_of(net);
  } else {
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, net.result()) << message_of(net);
    EXPECT_EQ(0u, message_of(net).find("No network data available")) << message_of(net);
  }

  collector->stop();
}
