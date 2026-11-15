// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <diagnostics/process_tables.hpp>
#include <string>
#include <vector>

using process_tables::process_row;

namespace {
process_row row(const std::string &name, const long long pid, const double cpu, const unsigned long long ws, const unsigned long long priv = 0) {
  process_row r;
  r.name = name;
  r.pid = pid;
  r.cpu = cpu;
  r.working_set = ws;
  r.private_bytes = priv;
  return r;
}

std::vector<std::string> names(const std::vector<process_row> &rows) {
  std::vector<std::string> out;
  for (const process_row &r : rows) out.push_back(r.name);
  return out;
}
}  // namespace

TEST(process_tables, ranks_by_cpu_and_by_working_set) {
  const std::vector<process_row> rows = {row("idle-ish", 10, 0.5, 100), row("acme-indexer", 20, 87.5, 2000), row("acme-cache", 30, 2.0, 9000),
                                         row("svchost", 40, 2.0, 500)};
  const process_tables::sample s = process_tables::rank(rows, 1000, 3);
  EXPECT_EQ(s.captured_at, 1000);
  EXPECT_EQ(s.total, 4u);
  // A tie on CPU goes to the larger process.
  EXPECT_EQ(names(s.top_cpu), (std::vector<std::string>{"acme-indexer", "acme-cache", "svchost"}));
  EXPECT_EQ(names(s.top_memory), (std::vector<std::string>{"acme-cache", "acme-indexer", "svchost"}));
}

TEST(process_tables, keeps_everything_when_there_are_fewer_than_asked) {
  const process_tables::sample s = process_tables::rank({row("a", 1, 1, 1)}, 5);
  EXPECT_EQ(s.top_cpu.size(), 1u);
  EXPECT_EQ(s.top_memory.size(), 1u);
}

TEST(process_tables, builds_both_sections_with_typed_columns) {
  const process_tables::sample s = process_tables::rank({row("acme-indexer", 20, 87.5, 2000, 1500), row("svchost", 40, 2.0, 500, 100)}, 1000, 1);
  PB::Diagnostics::DiagnosticsMessage message;
  process_tables::add_sections(&message, s);
  ASSERT_EQ(message.sections_size(), 2);
  const PB::Diagnostics::Section &cpu = message.sections(0);
  EXPECT_EQ(cpu.name(), "processes.top_cpu");
  EXPECT_EQ(cpu.captured_at(), 1000);
  ASSERT_EQ(cpu.columns_size(), 5);
  EXPECT_EQ(cpu.columns(0).name(), "name");
  EXPECT_EQ(cpu.columns(1).type(), PB::Diagnostics::Column::INT);
  EXPECT_EQ(cpu.columns(2).unit(), "percent");
  EXPECT_EQ(cpu.columns(3).unit(), "bytes");
  EXPECT_EQ(cpu.columns(4).name(), "private_bytes");
  ASSERT_EQ(cpu.rows_size(), 1);
  EXPECT_EQ(cpu.rows(0).cells(0).s(), "acme-indexer");
  EXPECT_EQ(cpu.rows(0).cells(1).i(), 20);
  EXPECT_DOUBLE_EQ(cpu.rows(0).cells(2).f(), 87.5);
  EXPECT_EQ(cpu.rows(0).cells(3).i(), 2000);
  EXPECT_EQ(cpu.rows(0).cells(4).i(), 1500);
  // Two processes, one row: a top list says so.
  EXPECT_TRUE(cpu.truncated());
  EXPECT_EQ(message.sections(1).name(), "processes.top_memory");
}

TEST(process_tables, adds_nothing_before_the_first_sample) {
  PB::Diagnostics::DiagnosticsMessage message;
  process_tables::add_sections(&message, process_tables::sample());
  EXPECT_EQ(message.sections_size(), 0);
}
