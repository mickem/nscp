// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <diagnostics/event_tables.hpp>
#include <string>
#include <vector>

using event_tables::event_row;

namespace {
event_row event(const std::int64_t time, const std::string &message, const std::string &key = "") {
  event_row e;
  e.time = time;
  e.message = message;
  e.key = key;
  return e;
}

std::vector<std::string> messages(const event_tables::ring::snapshot &s) {
  std::vector<std::string> out;
  for (const event_row &e : s.rows) out.push_back(e.message);
  return out;
}
}  // namespace

TEST(event_tables, ring_is_newest_first_and_drops_the_oldest) {
  event_tables::ring ring(3);
  for (int i = 1; i <= 5; ++i) ring.push(event(100 + i, "e" + std::to_string(i)));
  const event_tables::ring::snapshot s = ring.copy();
  EXPECT_EQ(messages(s), (std::vector<std::string>{"e5", "e4", "e3"}));
  EXPECT_EQ(s.seen, 5u);
  EXPECT_EQ(s.dropped, 2u);
}

TEST(event_tables, ring_orders_by_time_whatever_the_arrival_order) {
  // A seed read backwards and a live feed can interleave.
  event_tables::ring ring(10);
  ring.push(event(200, "live"));
  ring.push(event(150, "seed-2"));
  ring.push(event(100, "seed-1"));
  ring.push(event(200, "live-same-second"));
  EXPECT_EQ(messages(ring.copy()), (std::vector<std::string>{"live-same-second", "live", "seed-2", "seed-1"}));
}

TEST(event_tables, ring_holds_an_event_once) {
  event_tables::ring ring(10);
  EXPECT_TRUE(ring.push(event(100, "first", "System#42")));
  EXPECT_FALSE(ring.push(event(100, "again", "System#42")));
  EXPECT_TRUE(ring.push(event(100, "other log", "Application#42")));
  // No key, no identity: both are kept.
  EXPECT_TRUE(ring.push(event(101, "anonymous")));
  EXPECT_TRUE(ring.push(event(101, "anonymous")));
  EXPECT_EQ(ring.size(), 4u);
  EXPECT_EQ(ring.copy().seen, 4u);
}

TEST(event_tables, a_burst_cut_by_the_reader_still_counts) {
  event_tables::ring ring(10);
  ring.push(event(100, "kept"));
  ring.count_dropped(500);
  const event_tables::ring::snapshot s = ring.copy();
  EXPECT_EQ(s.rows.size(), 1u);
  EXPECT_EQ(s.seen, 501u);
  EXPECT_EQ(s.dropped, 500u);
}

TEST(event_tables, an_older_event_than_a_full_ring_holds_is_dropped) {
  event_tables::ring ring(2);
  ring.push(event(300, "new"));
  ring.push(event(200, "newer-ish"));
  ring.push(event(100, "too old"));
  const event_tables::ring::snapshot s = ring.copy();
  EXPECT_EQ(messages(s), (std::vector<std::string>{"new", "newer-ish"}));
  EXPECT_EQ(s.dropped, 1u);
}

TEST(event_tables, message_is_one_clipped_line) {
  EXPECT_EQ(event_tables::message("  The acme-sync service\r\nterminated\tunexpectedly.  \r\n"), "The acme-sync service terminated unexpectedly.");
  const std::string clipped = event_tables::message(std::string(5000, 'x'));
  EXPECT_EQ(clipped.size(), nscapi::diagnostics::max_cell_length);
  EXPECT_EQ(clipped.substr(clipped.size() - 3), "...");
  // Never inside a UTF-8 sequence.
  std::string wide;
  for (int i = 0; i < 600; ++i) wide += "\xC3\xA5";  // å
  const std::string w = event_tables::message(wide);
  EXPECT_LE(w.size(), nscapi::diagnostics::max_cell_length);
  EXPECT_EQ(w.substr(w.size() - 3), "...");
  EXPECT_EQ((w.size() - 3) % 2, 0u);
}

TEST(event_tables, service_events_table) {
  event_tables::ring ring;
  event_row e = event(1700000000, "The Acme catalogue sync service terminated unexpectedly.");
  e.service = "acme-sync";
  e.has_id = true;
  e.id = 7031;
  ring.push(e);
  event_row j = event(1700000010, "acme-sync.service: Failed with result 'exit-code'.");
  j.service = "acme-sync";
  ring.push(j);

  PB::Diagnostics::DiagnosticsMessage out;
  event_tables::add_service_events(&out, ring.copy(), event_tables::reader_state());
  ASSERT_EQ(out.sections_size(), 1);
  const PB::Diagnostics::Section &s = out.sections(0);
  EXPECT_EQ(s.name(), "services.events");
  std::vector<std::string> columns;
  for (const auto &c : s.columns()) columns.push_back(c.name());
  EXPECT_EQ(columns, (std::vector<std::string>{"time", "service", "id", "message"}));
  EXPECT_EQ(s.columns(0).type(), PB::Diagnostics::Column::TIMESTAMP);
  EXPECT_EQ(s.columns(2).type(), PB::Diagnostics::Column::INT);
  ASSERT_EQ(s.rows_size(), 2);
  // Newest first; a journal entry has no id.
  EXPECT_EQ(s.rows(0).cells(0).i(), 1700000010);
  EXPECT_EQ(s.rows(0).cells(2).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
  EXPECT_EQ(s.rows(1).cells(1).s(), "acme-sync");
  EXPECT_EQ(s.rows(1).cells(2).i(), 7031);
  EXPECT_FALSE(s.truncated());
}

TEST(event_tables, errors_table_carries_its_reader_state) {
  event_tables::ring ring(1);
  event_row a = event(100, "first");
  a.log = "Application";
  a.source = "Application Error";
  a.level = "error";
  a.has_id = true;
  a.id = 1000;
  ring.push(a);
  ring.push(event(101, "second"));

  event_tables::reader_state state;
  state.captured_at = 4242;
  PB::Diagnostics::DiagnosticsMessage out;
  event_tables::add_errors(&out, ring.copy(), state);
  const PB::Diagnostics::Section &s = out.sections(0);
  EXPECT_EQ(s.name(), "eventlog.errors");
  EXPECT_EQ(s.columns_size(), 6);
  EXPECT_EQ(s.captured_at(), 4242);
  // The ring let one go: the table holds "at least" its rows.
  EXPECT_TRUE(s.truncated());
  ASSERT_EQ(s.rows_size(), 1);
  EXPECT_EQ(s.rows(0).cells(5).s(), "second");

  state.error = "Failed to subscribe to System: access denied";
  PB::Diagnostics::DiagnosticsMessage failed;
  event_tables::add_errors(&failed, ring.copy(), state);
  EXPECT_EQ(failed.sections(0).error(), state.error);
  // One log failing does not hide what the other one collected.
  ASSERT_EQ(failed.sections(0).rows_size(), 1);
  EXPECT_EQ(failed.sections(0).rows(0).cells(5).s(), "second");
}
