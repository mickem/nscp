// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <diagnostics/service_tables.hpp>
#include <string>
#include <vector>

using service_tables::service_row;

namespace {
service_row row(const std::string &name, const long long exit_code = -1, const long long restarts = -1) {
  service_row r;
  r.name = name;
  r.state = "stopped";
  if (exit_code >= 0) {
    r.has_exit_code = true;
    r.exit_code = exit_code;
  }
  if (restarts >= 0) {
    r.has_restart_count = true;
    r.restart_count = restarts;
  }
  return r;
}

std::vector<std::string> names(const std::vector<service_row> &rows) {
  std::vector<std::string> out;
  for (const service_row &r : rows) out.push_back(r.name);
  return out;
}
}  // namespace

TEST(service_tables, restarting_then_failed_then_by_name) {
  const std::vector<service_row> rows = {row("zeta", 0), row("alpha", 0), row("crashed", 1067), row("acme-sync", 1, 12), row("flaky", 0, 2)};
  const service_tables::sample s = service_tables::make_sample(rows, 1000);
  EXPECT_EQ(s.captured_at, 1000);
  EXPECT_FALSE(s.truncated);
  EXPECT_EQ(names(s.rows), (std::vector<std::string>{"acme-sync", "flaky", "crashed", "alpha", "zeta"}));
}

TEST(service_tables, keeps_at_most_keep_rows_and_says_so) {
  const service_tables::sample s = service_tables::make_sample({row("c"), row("b"), row("a")}, 1000, 2);
  EXPECT_EQ(names(s.rows), (std::vector<std::string>{"a", "b"}));
  EXPECT_TRUE(s.truncated);
  PB::Diagnostics::DiagnosticsMessage out;
  service_tables::add_section(&out, s);
  ASSERT_EQ(out.sections_size(), 1);
  EXPECT_TRUE(out.sections(0).truncated());
}

TEST(service_tables, nothing_before_the_first_refresh) {
  PB::Diagnostics::DiagnosticsMessage out;
  service_tables::add_section(&out, service_tables::sample());
  EXPECT_EQ(out.sections_size(), 0);
}

TEST(service_tables, a_healthy_machine_is_an_empty_table) {
  PB::Diagnostics::DiagnosticsMessage out;
  service_tables::add_section(&out, service_tables::make_sample({}, 1234));
  ASSERT_EQ(out.sections_size(), 1);
  const PB::Diagnostics::Section &s = out.sections(0);
  EXPECT_EQ(s.name(), "services.problems");
  EXPECT_EQ(s.captured_at(), 1234);
  EXPECT_EQ(s.columns_size(), 7);
  EXPECT_EQ(s.rows_size(), 0);
  EXPECT_FALSE(s.truncated());
  EXPECT_TRUE(s.error().empty());
}

TEST(service_tables, unknown_values_are_null_cells) {
  service_row known = row("acme-sync", 1, 3);
  known.binary_path = "/opt/acme/bin/acme-sync";
  known.account = "root";
  known.started = 1700000000;
  PB::Diagnostics::DiagnosticsMessage out;
  service_tables::add_section(&out, service_tables::make_sample({known, row("plain")}, 1000));
  const PB::Diagnostics::Section &s = out.sections(0);
  ASSERT_EQ(s.rows_size(), 2);
  const PB::Diagnostics::Row &first = s.rows(0);
  ASSERT_EQ(first.cells_size(), 7);
  EXPECT_EQ(first.cells(0).s(), "acme-sync");
  EXPECT_EQ(first.cells(2).i(), 1);
  EXPECT_EQ(first.cells(3).s(), "/opt/acme/bin/acme-sync");
  EXPECT_EQ(first.cells(5).i(), 1700000000);
  EXPECT_EQ(first.cells(6).i(), 3);
  const PB::Diagnostics::Row &second = s.rows(1);
  ASSERT_EQ(second.cells_size(), 7);
  EXPECT_EQ(second.cells(2).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
  EXPECT_EQ(second.cells(5).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
  EXPECT_EQ(second.cells(6).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
}

TEST(service_tables, a_failed_refresh_is_an_error_not_a_healthy_machine) {
  PB::Diagnostics::DiagnosticsMessage out;
  service_tables::add_section(&out, service_tables::failed_sample("systemctl: not found", 1000));
  ASSERT_EQ(out.sections_size(), 1);
  EXPECT_EQ(out.sections(0).error(), "systemctl: not found");
  EXPECT_EQ(out.sections(0).rows_size(), 0);
}
