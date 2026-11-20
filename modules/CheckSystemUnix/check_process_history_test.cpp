// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_process_history / check_process_history_new, their metrics, and the
// two things only a running collector produces: the process history itself
// and the process tables of the diagnostics page. The real-time filter data
// sources (realtime_data.cpp) read the same collector, so they are driven
// here too.
//
// One collector is started for the whole suite - it needs two one-second
// ticks before it has anything to say - and every live case looks for the
// test binary itself, the one process certain to be running.

#include "check_process_history.h"

#include <gtest/gtest.h>
#include <limits.h>
#include <unistd.h>

#include <chrono>
#include <map>
#include <memory>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <string>
#include <thread>
#include <vector>

#include "check_test_support.hpp"
#include "realtime_data.hpp"
#include "realtime_thread.hpp"

using check_test::contains;
using check_test::make_request;
using check_test::message_of;
using check_test::response_type;
using process_history_check::history_type;
using process_history_check::process_record;

namespace {

// The name the collector records for the test binary: what its /proc/<pid>/exe
// link (Linux) or its process path (macOS) ends in.
std::string own_exe() {
  const int pid = static_cast<int>(getpid());
  for (const auto &p : check_proc::check_proc_filter::enumerate_processes()) {
    if (p.pid == pid) return p.exe;
  }
  return "";
}

bool in_history(const history_type &history, const std::string &exe) {
  for (const process_record &r : history) {
    if (r.exe == exe) return true;
  }
  return false;
}

response_type history(const std::shared_ptr<pdh_thread> &collector, const std::vector<std::string> &args) {
  response_type response;
  process_history_check::check_process_history(collector, make_request("check_process_history", args), &response);
  return response;
}

response_type history_new(const std::shared_ptr<pdh_thread> &collector, const std::vector<std::string> &args) {
  response_type response;
  process_history_check::check_process_history_new(collector, make_request("check_process_history_new", args), &response);
  return response;
}

// Build a filter of type F the way a command does and hand it to `use`.
template <class F, class Fn>
response_type with_filter(const std::string &command, const std::vector<std::string> &args, const std::string &detail, Fn use) {
  const auto request = make_request(command, args);
  response_type response;
  modern_filter::data_container data;
  modern_filter::cli_helper<F> helper(request, &response, data);
  F filter;
  helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${list}", detail, "${count}", "empty", "");
  if (!helper.parse_options()) return response;
  if (!helper.build_filter(filter)) return response;
  use(filter);
  helper.post_process(filter);
  return response;
}

class LiveCollector : public ::testing::Test {
 protected:
  static std::shared_ptr<pdh_thread> collector;
  static std::string exe;

  static void SetUpTestSuite() {
    exe = own_exe();
    collector = std::make_shared<pdh_thread>();
    collector->process_history_enabled = true;
    collector->diagnostics_processes = true;
    collector->start();
    // Bounded: the first tick takes the baselines, the second has a sample,
    // a history entry and a ranked process table.
    for (int i = 0; i < 150; ++i) {
      if (collector->cpu_status().state == sampling::readiness::ready && collector->memory_status().state == sampling::readiness::ready &&
          collector->get_process_tables().captured_at != 0 && in_history(collector->get_process_history(), exe))
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  static void TearDownTestSuite() {
    collector->stop();
    collector.reset();
  }
};

std::shared_ptr<pdh_thread> LiveCollector::collector;
std::string LiveCollector::exe;

}  // namespace

// --- without a usable collector ----------------------------------------------

TEST(CheckProcessHistory, MissingCollectorIsUnknown) {
  const response_type r = history(nullptr, {});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
  EXPECT_EQ("Process history collector not initialized", message_of(r));

  const response_type n = history_new(nullptr, {});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, n.result());
  EXPECT_EQ("Process history collector not initialized", message_of(n));
}

TEST(CheckProcessHistory, DisabledHistoryNamesTheSettingToChange) {
  const auto collector = std::make_shared<pdh_thread>();
  EXPECT_EQ("Process history is not enabled (set 'process history = true' under the module settings)", message_of(history(collector, {})));

  collector->set_settings_path("/settings/system/unix");
  const response_type r = history_new(collector, {});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
  EXPECT_EQ("Process history is not enabled (set 'process history = true' under /settings/system/unix)", message_of(r));
}

TEST(CheckProcessHistory, ABadFilterIsReportedBeforeTheCollectorIsAsked) {
  // No collector at all: a syntax error must still be the answer, not the
  // "not initialized" that would hide it.
  const response_type r = history(nullptr, {"filter=no_such_keyword = 1"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result());
  EXPECT_NE("Process history collector not initialized", message_of(r));
}

TEST(ProcessRecord, SeenTimesRenderAsDatesOrNever) {
  process_record never;
  EXPECT_EQ("never", never.get_first_seen_s());
  EXPECT_EQ("never", never.get_last_seen_s());
  EXPECT_EQ("false", never.get_currently_running());

  const process_record seen("bash", 86400);
  EXPECT_NE("never", seen.get_first_seen_s());
  EXPECT_TRUE(contains(seen.get_first_seen_s(), "1970")) << seen.get_first_seen_s();
  EXPECT_EQ(seen.get_first_seen_s(), seen.get_last_seen_s());
  EXPECT_EQ(1, seen.get_times_seen());
  EXPECT_EQ(1, seen.get_currently_running_i());
}

// --- metrics -----------------------------------------------------------------

TEST(ProcessHistoryMetrics, OneInstancePerProcessPlusTheTotals) {
  history_type data;
  process_record running("nginx", 100);
  running.times_seen = 3;
  process_record gone("backup", 100);
  gone.currently_running = false;
  data.push_back(running);
  data.push_back(gone);

  PB::Metrics::MetricsBundle parent;
  process_history_check::build_process_history_metrics(&parent, data);
  ASSERT_EQ(1, parent.children_size());
  const PB::Metrics::MetricsBundle &section = parent.children(0);
  EXPECT_EQ("process_history", section.key());

  std::map<std::string, const PB::Metrics::Metric *> by_key;
  for (const auto &m : section.value()) by_key[m.key()] = &m;

  // times_seen only ever grows while the agent runs: a counter. The rest can
  // go down again, so they are gauges whatever their names suggest.
  ASSERT_TRUE(by_key.count("nginx.times_seen"));
  EXPECT_TRUE(by_key["nginx.times_seen"]->has_counter_value());
  double times_seen = 0;
  ASSERT_TRUE(nscapi::metrics::numeric_value(*by_key["nginx.times_seen"], times_seen));
  EXPECT_DOUBLE_EQ(3, times_seen);
  ASSERT_TRUE(by_key.count("backup.currently_running"));
  EXPECT_TRUE(by_key["backup.currently_running"]->has_gauge_value());
  EXPECT_DOUBLE_EQ(0, by_key["backup.currently_running"]->gauge_value().value());
  EXPECT_DOUBLE_EQ(1, by_key["nginx.currently_running"]->gauge_value().value());

  ASSERT_TRUE(by_key.count("count"));
  EXPECT_DOUBLE_EQ(2, by_key["count"]->gauge_value().value());
  ASSERT_TRUE(by_key.count("running"));
  EXPECT_DOUBLE_EQ(1, by_key["running"]->gauge_value().value());

  // The process rides a label, so the exposition has one family per metric
  // rather than one per process.
  ASSERT_EQ(1, by_key["nginx.times_seen"]->dims_size());
  EXPECT_EQ("exe", by_key["nginx.times_seen"]->dims(0).key());
  EXPECT_EQ("nginx", by_key["nginx.times_seen"]->dims(0).value());
}

TEST(ProcessHistoryMetrics, AnEmptyHistoryStillReportsItsTotals) {
  PB::Metrics::MetricsBundle parent;
  process_history_check::build_process_history_metrics(&parent, history_type());
  ASSERT_EQ(1, parent.children_size());
  EXPECT_EQ(2, parent.children(0).value_size());
}

// --- against a running collector ----------------------------------------------

TEST_F(LiveCollector, HistoryRecordsTheTestBinaryOnce) {
  ASSERT_FALSE(exe.empty());
  const history_type data = collector->get_process_history();
  ASSERT_TRUE(in_history(data, exe)) << exe << " is not in a history of " << data.size();
  for (const process_record &r : data) {
    if (r.exe != exe) continue;
    // Seen on every tick since, but started only once.
    EXPECT_EQ(1, r.times_seen);
    EXPECT_TRUE(r.currently_running);
    EXPECT_GT(r.first_seen, 0);
    EXPECT_GE(r.last_seen, r.first_seen);
  }
}

TEST_F(LiveCollector, CheckProcessHistoryRendersARequestedProcess) {
  const response_type r = history(collector, {"process=" + exe, "top-syntax=${list}", "detail-syntax=${exe}|${times_seen}|${running}|${currently_running}"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_EQ(exe + "|1|true|1", message_of(r));
}

TEST_F(LiveCollector, CheckProcessHistoryDefaultSummaryCountsTheHistory) {
  const response_type r = history(collector, {});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.result()) << message_of(r);
  EXPECT_EQ(0u, message_of(r).find("OK: ")) << message_of(r);
  EXPECT_TRUE(contains(message_of(r), " processes in history.")) << message_of(r);
}

TEST_F(LiveCollector, AProcessNeverSeenIsListedAsNeverSeen) {
  const response_type r = history(collector, {"process=nscp-no-such-process-b3f1", "top-syntax=${list}", "detail-syntax=${exe}|${first_seen}|${running}"});
  EXPECT_EQ("nscp-no-such-process-b3f1|never|false", message_of(r));
}

TEST_F(LiveCollector, ThresholdsApplyToTheHistory) {
  const response_type r = history(collector, {"process=" + exe, "critical=times_seen >= 1", "top-syntax=${status}: ${problem_list}", "detail-syntax=${exe}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.result()) << message_of(r);
  EXPECT_EQ("CRITICAL: " + exe, message_of(r));
}

TEST_F(LiveCollector, CheckProcessHistoryNewListsWhatStartedInsideTheWindow) {
  const response_type recent = history_new(collector, {"time=5m", "detail-syntax=${exe}"});
  EXPECT_EQ(PB::Common::ResultCode::OK, recent.result()) << message_of(recent);
  EXPECT_TRUE(contains(message_of(recent), exe)) << message_of(recent);
}

TEST_F(LiveCollector, CheckProcessHistoryNewRejectsABadWindow) {
  const response_type r = history_new(collector, {"time=soon"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.result()) << message_of(r);
  EXPECT_EQ(0u, message_of(r).find("Invalid time window 'soon'")) << message_of(r);
}

TEST_F(LiveCollector, ProcessTablesRankTheMachine) {
  const process_tables::sample tables = collector->get_process_tables();
  EXPECT_NE(0, tables.captured_at);
  EXPECT_EQ("", tables.error);
  EXPECT_GT(tables.total, 0u);
  EXPECT_FALSE(tables.top_memory.empty());
  EXPECT_LE(tables.top_memory.size(), tables.total);
}

// --- the real-time filter data sources -----------------------------------------

TEST_F(LiveCollector, RealtimeCpuDataRendersEveryRequestedWindow) {
  checks::check_cpu_filter::runtime_data data;
  data.add("10s");
  data.add("1m");
  ASSERT_EQ(2u, data.checks.size());
  EXPECT_EQ("10s", data.checks.front().alias);
  EXPECT_EQ(10, data.checks.front().time);
  EXPECT_EQ(60, data.checks.back().time);

  modern_filter::match_result matched;
  const response_type r = with_filter<checks::check_cpu_filter::filter>(
      "check_cpu", {"filter=core = 'total'"}, "${time}", [&](checks::check_cpu_filter::filter &f) { matched = data.process_item(f, collector.get()); });
  EXPECT_TRUE(matched.matched_filter);
  EXPECT_EQ("10s, 1m", message_of(r));
}

TEST_F(LiveCollector, RealtimeCpuDataHasNothingToSayWithoutACollector) {
  checks::check_cpu_filter::runtime_data data;
  data.add("1m");
  modern_filter::match_result matched;
  const response_type r = with_filter<checks::check_cpu_filter::filter>("check_cpu", {}, "${time}", [&](checks::check_cpu_filter::filter &f) {
    matched = data.process_item(f, nullptr);
    // A collector that has not sampled yet is the same: no data, no match.
    pdh_thread cold;
    matched.append(data.process_item(f, &cold));
  });
  EXPECT_FALSE(matched.matched_filter);
  EXPECT_EQ("empty", message_of(r));
}

TEST_F(LiveCollector, RealtimeMemoryDataSkipsTypesItDoesNotKnow) {
  check_memory::check_mem_filter::runtime_data data;
  data.add("physical");
  data.add("nonsense");
  data.add("cached");
  data.add("swap");

  modern_filter::match_result matched;
  const response_type r = with_filter<check_memory::check_mem_filter::filter>(
      "check_memory", {}, "${type}", [&](check_memory::check_mem_filter::filter &f) { matched = data.process_item(f, collector.get()); });
  EXPECT_TRUE(matched.matched_filter);
  EXPECT_EQ("physical, cached, swap", message_of(r));

  pdh_thread cold;
  modern_filter::match_result none;
  with_filter<check_memory::check_mem_filter::filter>("check_memory", {}, "${type}",
                                                      [&](check_memory::check_mem_filter::filter &f) { none = data.process_item(f, &cold); });
  EXPECT_FALSE(none.matched_filter);
}

TEST_F(LiveCollector, RealtimeProcessDataMatchesNamesWithoutRegardToCase) {
  std::string upper = exe;
  for (char &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

  check_proc::check_proc_filter::runtime_data named;
  named.add(upper);
  EXPECT_FALSE(named.check_all);
  ASSERT_EQ(1u, named.checks.size());

  modern_filter::match_result matched;
  const response_type r = with_filter<check_proc::check_proc_filter::filter>(
      "check_process", {}, "${exe}", [&](check_proc::check_proc_filter::filter &f) { matched = named.process_item(f, collector.get()); });
  EXPECT_TRUE(matched.matched_filter);
  EXPECT_EQ(exe, message_of(r));
}

TEST_F(LiveCollector, RealtimeProcessDataStarMeansEveryProcess) {
  check_proc::check_proc_filter::runtime_data all;
  all.add("*");
  EXPECT_TRUE(all.check_all);
  EXPECT_TRUE(all.checks.empty());

  modern_filter::match_result matched;
  const response_type r =
      with_filter<check_proc::check_proc_filter::filter>("check_process", {"filter=pid = " + std::to_string(getpid())}, "${exe}",
                                                         [&](check_proc::check_proc_filter::filter &f) { matched = all.process_item(f, nullptr); });
  EXPECT_TRUE(matched.matched_filter);
  EXPECT_EQ(exe, message_of(r));
}
