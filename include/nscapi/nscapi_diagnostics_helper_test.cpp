// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <stdexcept>
#include <string>

using nscapi::diagnostics::table;
using nscapi::diagnostics::type_bool;
using nscapi::diagnostics::type_float;
using nscapi::diagnostics::type_int;
using nscapi::diagnostics::type_timestamp;

namespace {
PB::Diagnostics::DiagnosticsMessage processes() {
  PB::Diagnostics::DiagnosticsMessage message;
  auto t = table(&message, "processes.top_cpu").describe("Processes using the most CPU");
  t.column("name").column("pid", type_int).column("cpu", type_float, "percent").column("working_set", type_int, "bytes");
  t.row().add("acme-indexer").add(4242).add(87.5).add(1048576ULL);
  t.row().add(std::string("svchost.exe")).add(17L).add(0.5).add(2048);
  return message;
}
}  // namespace

TEST(diagnostics_helper, builds_typed_columns_and_rows_in_order) {
  const PB::Diagnostics::DiagnosticsMessage message = processes();
  ASSERT_EQ(message.sections_size(), 1);
  const PB::Diagnostics::Section &s = message.sections(0);
  EXPECT_EQ(s.name(), "processes.top_cpu");
  EXPECT_EQ(s.description(), "Processes using the most CPU");
  EXPECT_GT(s.captured_at(), 0);
  EXPECT_FALSE(s.truncated());
  ASSERT_EQ(s.columns_size(), 4);
  EXPECT_EQ(s.columns(0).type(), PB::Diagnostics::Column::STRING);
  EXPECT_EQ(s.columns(1).type(), PB::Diagnostics::Column::INT);
  EXPECT_EQ(s.columns(2).type(), PB::Diagnostics::Column::FLOAT);
  EXPECT_EQ(s.columns(2).unit(), "percent");
  EXPECT_EQ(s.columns(3).unit(), "bytes");
  ASSERT_EQ(s.rows_size(), 2);
  EXPECT_EQ(s.rows(0).cells(0).s(), "acme-indexer");
  EXPECT_EQ(s.rows(0).cells(1).i(), 4242);
  EXPECT_DOUBLE_EQ(s.rows(0).cells(2).f(), 87.5);
  EXPECT_EQ(s.rows(0).cells(3).i(), 1048576);
  EXPECT_EQ(s.rows(1).cells(0).s(), "svchost.exe");
}

TEST(diagnostics_helper, pads_a_short_row_with_empty_cells) {
  PB::Diagnostics::DiagnosticsMessage message;
  {
    auto t = table(&message, "x");
    t.column("a").column("b", type_int).column("c", type_bool);
    t.row().add("first");
    t.row().add("second").add(2);
  }
  const PB::Diagnostics::Section &s = message.sections(0);
  ASSERT_EQ(s.rows_size(), 2);
  // Padded when the next row starts...
  ASSERT_EQ(s.rows(0).cells_size(), 3);
  EXPECT_EQ(s.rows(0).cells(1).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
  // ...and the last one when the builder goes.
  ASSERT_EQ(s.rows(1).cells_size(), 3);
  EXPECT_EQ(s.rows(1).cells(1).i(), 2);
  EXPECT_EQ(s.rows(1).cells(2).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
}

TEST(diagnostics_helper, refuses_a_cell_past_the_last_column) {
  PB::Diagnostics::DiagnosticsMessage message;
  auto t = table(&message, "x");
  t.column("a").column("b");
  EXPECT_THROW(t.row().add("1").add("2").add("3"), std::length_error);
}

TEST(diagnostics_helper, refuses_a_row_before_the_columns_and_a_column_after_a_row) {
  PB::Diagnostics::DiagnosticsMessage message;
  auto t = table(&message, "x");
  EXPECT_THROW(t.row(), std::logic_error);
  t.column("a");
  t.row().add("1");
  EXPECT_THROW(t.column("b"), std::logic_error);
}

TEST(diagnostics_helper, clips_a_long_cell_and_marks_the_section) {
  PB::Diagnostics::DiagnosticsMessage message;
  {
    auto t = table(&message, "x");
    t.column("message");
    t.row().add(std::string(5000, 'a'));
  }
  const PB::Diagnostics::Section &s = message.sections(0);
  const std::string &cell = s.rows(0).cells(0).s();
  EXPECT_EQ(cell.size(), nscapi::diagnostics::max_cell_length);
  EXPECT_EQ(cell.substr(cell.size() - 3), "...");
  EXPECT_TRUE(s.truncated());
}

TEST(diagnostics_helper, clips_on_a_utf8_boundary) {
  // "ö" is two bytes (C3 B6): a cut that would split one steps back over it.
  const std::string text = "ab\xC3\xB6\xC3\xB6\xC3\xB6";
  const std::string clipped = nscapi::diagnostics::clip(text, 6);
  // 6 - 3 = 3 bytes of text would end inside the first "ö", so only "ab" stays.
  EXPECT_EQ(clipped, "ab...");
  EXPECT_EQ(nscapi::diagnostics::clip("short", 10), "short");
  EXPECT_EQ(nscapi::diagnostics::clip(text, 7), "ab\xC3\xB6...");
}

TEST(diagnostics_helper, keeps_at_most_max_rows_and_marks_the_rest_truncated) {
  PB::Diagnostics::DiagnosticsMessage message;
  {
    auto t = table(&message, "x");
    t.column("n", type_int);
    for (int i = 0; i < 70; ++i) t.row().add(i);
  }
  const PB::Diagnostics::Section &s = message.sections(0);
  EXPECT_EQ(static_cast<std::size_t>(s.rows_size()), nscapi::diagnostics::max_rows);
  EXPECT_TRUE(s.truncated());
  EXPECT_EQ(s.rows(0).cells(0).i(), 0);
}

TEST(diagnostics_helper, carries_times_nulls_errors_and_saturates_huge_unsigned) {
  PB::Diagnostics::DiagnosticsMessage message;
  {
    auto t = table(&message, "x").captured_at(1000).error("could not read");
    t.column("when", type_timestamp).column("missing").column("big", type_int).column("ok", type_bool);
    t.row().add_time(1700000000).add_null().add(18446744073709551615ULL).add(true);
  }
  const PB::Diagnostics::Section &s = message.sections(0);
  EXPECT_EQ(s.captured_at(), 1000);
  EXPECT_EQ(s.error(), "could not read");
  EXPECT_EQ(s.columns(0).type(), PB::Diagnostics::Column::TIMESTAMP);
  EXPECT_EQ(s.rows(0).cells(0).i(), 1700000000);
  EXPECT_EQ(s.rows(0).cells(1).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
  EXPECT_EQ(s.rows(0).cells(2).i(), INT64_MAX);
  EXPECT_TRUE(s.rows(0).cells(3).b());
}
