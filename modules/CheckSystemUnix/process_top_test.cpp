// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "process_top.h"

#include <gtest/gtest.h>

#include <string>

namespace {
// Captured from a real host: postgres, then a name with blanks and
// parentheses, which is why the name runs to the *last* ')'.
const char *const postgres_stat =
    "1234 (postgres) S 1 1234 1234 0 -1 4194560 2048 0 12 0 150 50 0 0 20 0 1 0 98765 228323328 4096 18446744073709551615 1 1 0 0 0 0 0 4096 "
    "16384 0 0 0 17 2 0 0 0 0 0 0 0 0 0 0 0 0 0";
const char *const odd_stat = "77 (my (odd) proc) R 1 77 77 0 -1 0 0 0 0 0 7 3 0 0 20 0 1 0 555 1000 10 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0";

process_top::process_times process(const std::string &name, const unsigned long long start, const unsigned long long ticks, const unsigned long long ws = 0) {
  process_top::process_times p;
  p.name = name;
  p.start = start;
  p.ticks = ticks;
  p.working_set = ws;
  return p;
}
}  // namespace

TEST(process_top, parses_the_name_the_cpu_ticks_and_the_start) {
  process_top::process_times p;
  ASSERT_TRUE(process_top::parse_stat(postgres_stat, p));
  EXPECT_EQ(p.name, "postgres");
  EXPECT_EQ(p.ticks, 200u);
  EXPECT_EQ(p.start, 98765u);
  ASSERT_TRUE(process_top::parse_stat(odd_stat, p));
  EXPECT_EQ(p.name, "my (odd) proc");
  EXPECT_EQ(p.ticks, 10u);
  EXPECT_EQ(p.start, 555u);
}

TEST(process_top, refuses_a_truncated_or_empty_stat) {
  process_top::process_times p;
  EXPECT_FALSE(process_top::parse_stat("", p));
  EXPECT_FALSE(process_top::parse_stat("12 (gone) S 1 2 3", p));
  EXPECT_FALSE(process_top::parse_stat("no parentheses at all", p));
}

TEST(process_top, reads_resident_and_private_memory_from_statm) {
  process_top::process_times p;
  ASSERT_TRUE(process_top::parse_statm("55741 4096 1024 1 0 9000 0\n", 4096, p));
  EXPECT_EQ(p.working_set, 4096u * 4096u);
  EXPECT_EQ(p.private_bytes, 3072u * 4096u);
  EXPECT_FALSE(process_top::parse_statm("", 4096, p));
}

TEST(process_top, gives_a_cpu_share_only_to_a_process_seen_twice) {
  process_top::table prev, cur;
  prev[10] = process("steady", 100, 1000);
  prev[20] = process("reused", 100, 5000);
  cur[10] = process("steady", 100, 1050, 2048);
  // The pid was reused: a different start, so no share from the old counters.
  cur[20] = process("reused", 999, 10);
  cur[30] = process("new", 300, 40);
  const std::vector<process_tables::process_row> rows = process_top::rows(prev, cur, 400);
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0].name, "steady");
  EXPECT_EQ(rows[0].pid, 10);
  // 50 of the 400 ticks every core spent: one core of four, half busy.
  EXPECT_DOUBLE_EQ(rows[0].cpu, 12.5);
  EXPECT_EQ(rows[0].working_set, 2048u);
  EXPECT_DOUBLE_EQ(rows[1].cpu, 0.0);
  EXPECT_DOUBLE_EQ(rows[2].cpu, 0.0);
}

TEST(process_top, no_interval_is_no_share) {
  process_top::table prev, cur;
  prev[10] = process("a", 1, 10);
  cur[10] = process("a", 1, 20);
  EXPECT_DOUBLE_EQ(process_top::rows(prev, cur, 0)[0].cpu, 0.0);
}
