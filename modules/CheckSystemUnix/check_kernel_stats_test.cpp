/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "check_kernel_stats.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <map>

namespace fs = boost::filesystem;
using kernel_stats_check::build_rows;
using kernel_stats_check::kstat_counters;
using kernel_stats_check::kstat_row;
using kernel_stats_check::parse_proc_stat_counters;

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

std::map<std::string, kstat_row> by_name(const kernel_stats_check::rows_type &rows) {
  std::map<std::string, kstat_row> m;
  for (const kstat_row &r : rows) m[r.name] = r;
  return m;
}

PB::Common::ResultCode run_ks(long long thread_count, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  kstat_counters prev;
  prev.ctxt = 1000;
  prev.processes = 100;
  prev.valid = true;
  kstat_counters cur;
  cur.ctxt = 3000;
  cur.processes = 120;
  cur.valid = true;
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_kernel_stats");
  for (const std::string &a : args) request.add_arguments(a);
  kernel_stats_check::check_kernel_stats_from(request, &response, prev, cur, 2.0, thread_count);
  return response.result();
}
}  // namespace

TEST(CheckKernelStats, ParsesCounters) {
  const kstat_counters c = parse_proc_stat_counters("cpu 1 2 3\nctxt 123456\nbtime 111\nprocesses 789\nprocs_running 2\n");
  ASSERT_TRUE(c.valid);
  EXPECT_EQ(c.ctxt, 123456u);
  EXPECT_EQ(c.processes, 789u);
}

TEST(CheckKernelStats, MissingProcessesIsInvalid) {
  EXPECT_FALSE(parse_proc_stat_counters("ctxt 5\n").valid);
}

TEST(CheckKernelStats, BuildsRatesOverElapsed) {
  kstat_counters prev;
  prev.ctxt = 1000;
  prev.processes = 100;
  prev.valid = true;
  kstat_counters cur;
  cur.ctxt = 1200;   // +200 over 2s -> 100/s
  cur.processes = 110;  // +10 over 2s -> 5/s
  cur.valid = true;

  const auto rows = by_name(build_rows(prev, cur, 2.0, 337, {}));
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_DOUBLE_EQ(rows.at("ctxt").rate, 100.0);
  EXPECT_EQ(rows.at("ctxt").current, 1200);
  EXPECT_DOUBLE_EQ(rows.at("processes").rate, 5.0);
  EXPECT_DOUBLE_EQ(rows.at("threads").rate, 0.0);
  EXPECT_EQ(rows.at("threads").current, 337);
  EXPECT_EQ(rows.at("threads").label, "Threads");
}

TEST(CheckKernelStats, TypeSelectsSubset) {
  kstat_counters prev, cur;
  prev.valid = cur.valid = true;
  const auto rows = build_rows(prev, cur, 1.0, 10, {"threads"});
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows.front().name, "threads");
}

TEST(CheckKernelStats, CounterResetGivesZeroRate) {
  kstat_counters prev;
  prev.ctxt = 5000;
  prev.valid = true;
  kstat_counters cur;  // ctxt reset to 0
  cur.valid = true;
  const auto rows = by_name(build_rows(prev, cur, 1.0, 1, {"ctxt"}));
  EXPECT_DOUBLE_EQ(rows.at("ctxt").rate, 0.0);
}

TEST(CheckKernelStats, CountsThreadsFromFakeProc) {
  const fs::path root = fs::temp_directory_path() / fs::unique_path("nscp-proc-%%%%-%%%%");
  fs::create_directories(root / "123" / "task" / "123");
  fs::create_directories(root / "123" / "task" / "124");
  fs::create_directories(root / "456" / "task" / "456");
  fs::create_directories(root / "self" / "task" / "1");  // non-numeric pid -> ignored
  EXPECT_EQ(kernel_stats_check::count_threads_from(root.string()), 3);
  fs::remove_all(root);
}

TEST(CheckKernelStats, DefaultThresholdTargetsThreads) {
  PB::Commands::QueryResponseMessage::Response ok;
  EXPECT_EQ(run_ks(500, {}, ok), PB::Common::ResultCode::OK) << join_lines(ok);

  PB::Commands::QueryResponseMessage::Response warn;
  EXPECT_EQ(run_ks(9000, {}, warn), PB::Common::ResultCode::WARNING) << join_lines(warn);

  PB::Commands::QueryResponseMessage::Response crit;
  EXPECT_EQ(run_ks(11000, {}, crit), PB::Common::ResultCode::CRITICAL) << join_lines(crit);
}

TEST(CheckKernelStats, HugeCtxtCounterDoesNotTripThreadThreshold) {
  // ctxt current is in the billions but the default threshold only applies to
  // the threads row, so a normal thread count stays OK.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_ks(200, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}
