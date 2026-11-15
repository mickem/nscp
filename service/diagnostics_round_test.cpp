// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "diagnostics_round.hpp"

#include <gtest/gtest.h>

#include <string>

using nsclient::core::diagnostics_snapshot;
using nsclient::core::failed_check_store;
namespace dround = nsclient::core::diagnostics_round;

namespace {
// A section of `rows` rows of one string column holding `text`.
void add_section(PB::Diagnostics::DiagnosticsMessage &m, const std::string &name, const int rows, const std::string &text = "x") {
  auto t = nscapi::diagnostics::table(&m, name);
  t.column("value");
  for (int i = 0; i < rows; ++i) t.row().add(text);
}

failed_check_store::entry failure(const std::int64_t when, const std::string &command, const PB::Common::ResultCode code, const std::string &message) {
  failed_check_store::entry e;
  e.set_timestamp(when);
  e.set_subject("Scheduler");
  e.mutable_response()->set_command(command);
  e.mutable_response()->set_result(code);
  e.mutable_response()->add_lines()->set_message(message);
  return e;
}
}  // namespace

TEST(diagnostics_round, recent_failures_lists_the_log_newest_first_with_the_first_line) {
  failed_check_store::entry_list entries;
  entries.push_back(failure(200, "check_service", PB::Common::ResultCode::CRITICAL, "acme-sync is stopped\nsecond line"));
  entries.push_back(failure(100, "check_drivesize", PB::Common::ResultCode::WARNING, "C: 91% used"));
  PB::Diagnostics::DiagnosticsMessage m;
  dround::add_recent_failures(m, entries, 10, 300);
  ASSERT_EQ(m.sections_size(), 1);
  const PB::Diagnostics::Section &s = m.sections(0);
  EXPECT_EQ(s.name(), "failures.recent");
  EXPECT_EQ(s.source(), "core");
  EXPECT_EQ(s.captured_at(), 300);
  ASSERT_EQ(s.columns_size(), 5);
  EXPECT_EQ(s.columns(0).type(), PB::Diagnostics::Column::TIMESTAMP);
  ASSERT_EQ(s.rows_size(), 2);
  EXPECT_EQ(s.rows(0).cells(0).i(), 200);
  EXPECT_EQ(s.rows(0).cells(1).s(), "check_service");
  EXPECT_EQ(s.rows(0).cells(2).s(), "CRITICAL");
  EXPECT_EQ(s.rows(0).cells(3).s(), "Scheduler");
  EXPECT_EQ(s.rows(0).cells(4).s(), "acme-sync is stopped");
  EXPECT_EQ(s.rows(1).cells(2).s(), "WARNING");
  EXPECT_FALSE(s.truncated());
}

TEST(diagnostics_round, recent_failures_says_when_there_were_more) {
  failed_check_store::entry_list entries;
  for (int i = 0; i < 4; ++i) entries.push_back(failure(100 - i, "check_x", PB::Common::ResultCode::CRITICAL, "m"));
  PB::Diagnostics::DiagnosticsMessage m;
  dround::add_recent_failures(m, entries, 3, 300);
  EXPECT_EQ(m.sections(0).rows_size(), 3);
  EXPECT_TRUE(m.sections(0).truncated());
}

TEST(diagnostics_round, an_empty_log_is_an_empty_table_not_a_missing_one) {
  PB::Diagnostics::DiagnosticsMessage m;
  dround::add_recent_failures(m, failed_check_store::entry_list(), 10, 300);
  ASSERT_EQ(m.sections_size(), 1);
  EXPECT_EQ(m.sections(0).rows_size(), 0);
  EXPECT_EQ(m.sections(0).columns_size(), 5);
}

TEST(diagnostics_round, cuts_rows_to_the_cap_and_marks_it) {
  PB::Diagnostics::DiagnosticsMessage m;
  add_section(m, "processes.top_cpu", 30);
  dround::caps limits;
  limits.max_rows = 10;
  const diagnostics_snapshot s = dround::assemble(m, limits, 1);
  ASSERT_EQ(s.message.sections_size(), 1);
  EXPECT_EQ(s.message.sections(0).rows_size(), 10);
  EXPECT_TRUE(s.message.sections(0).truncated());
  EXPECT_EQ(s.max_rows, 10u);
  EXPECT_EQ(s.captured_at, 1);
}

TEST(diagnostics_round, redacts_credentials_in_every_cell_and_error) {
  PB::Diagnostics::DiagnosticsMessage m;
  add_section(m, "eventlog.errors", 1, "login failed for password=hunter2 user=bob");
  m.mutable_sections(0)->set_error("token=abc could not be used");
  const diagnostics_snapshot s = dround::assemble(m, dround::caps(), 1);
  const PB::Diagnostics::Section &section = s.message.sections(0);
  EXPECT_EQ(section.rows(0).cells(0).s(), "login failed for password=*** user=bob");
  EXPECT_EQ(section.error(), "token=*** could not be used");
}

TEST(diagnostics_round, clips_a_cell_a_producer_did_not) {
  PB::Diagnostics::DiagnosticsMessage m;
  PB::Diagnostics::Section *section = m.add_sections();
  section->set_name("raw");
  section->add_columns()->set_name("value");
  section->add_rows()->add_cells()->set_s(std::string(4000, 'a'));
  const diagnostics_snapshot s = dround::assemble(m, dround::caps(), 1);
  EXPECT_EQ(s.message.sections(0).rows(0).cells(0).s().size(), nscapi::diagnostics::max_cell_length);
  EXPECT_TRUE(s.message.sections(0).truncated());
}

TEST(diagnostics_round, squares_a_ragged_row) {
  PB::Diagnostics::DiagnosticsMessage m;
  PB::Diagnostics::Section *section = m.add_sections();
  section->set_name("raw");
  section->add_columns()->set_name("a");
  section->add_columns()->set_name("b");
  PB::Diagnostics::Row *wide = section->add_rows();
  for (int i = 0; i < 3; ++i) wide->add_cells()->set_i(i);
  section->add_rows()->add_cells()->set_i(9);
  const diagnostics_snapshot s = dround::assemble(m, dround::caps(), 1);
  const PB::Diagnostics::Section &out = s.message.sections(0);
  EXPECT_EQ(out.rows(0).cells_size(), 2);
  EXPECT_EQ(out.rows(1).cells_size(), 2);
  EXPECT_TRUE(out.truncated());
}

TEST(diagnostics_round, prune_pattern_leaves_a_section_out_and_says_so) {
  PB::Diagnostics::DiagnosticsMessage m;
  add_section(m, "eventlog.errors", 1);
  add_section(m, "processes.top_cpu", 1);
  dround::caps limits;
  limits.prune_patterns = nsclient::core::metrics_prune::parse_patterns("EventLog");
  const diagnostics_snapshot s = dround::assemble(m, limits, 1);
  ASSERT_EQ(s.message.sections_size(), 1);
  EXPECT_EQ(s.message.sections(0).name(), "processes.top_cpu");
  ASSERT_EQ(s.excluded.size(), 1u);
  EXPECT_EQ(s.excluded[0], "eventlog.errors");
}

TEST(diagnostics_round, keeps_the_first_of_two_sections_with_one_name_and_at_most_the_section_cap) {
  PB::Diagnostics::DiagnosticsMessage m;
  add_section(m, "same", 1, "first");
  add_section(m, "same", 1, "second");
  for (int i = 0; i < 20; ++i) add_section(m, "s" + std::to_string(i), 0);
  const diagnostics_snapshot s = dround::assemble(m, dround::caps(), 1);
  EXPECT_EQ(static_cast<std::size_t>(s.message.sections_size()), nscapi::diagnostics::max_sections);
  EXPECT_EQ(s.message.sections(0).rows(0).cells(0).s(), "first");
  // The duplicate, and the 5 past the cap of 16.
  EXPECT_EQ(s.dropped, 1u + 21u - nscapi::diagnostics::max_sections);
}

TEST(diagnostics_round, trims_the_longest_section_until_the_snapshot_fits) {
  PB::Diagnostics::DiagnosticsMessage m;
  add_section(m, "small", 2, std::string(100, 's'));
  add_section(m, "large", 10, std::string(1000, 'l'));
  dround::caps limits;
  limits.max_size = 6000;
  const diagnostics_snapshot s = dround::assemble(m, limits, 1);
  EXPECT_LE(s.message.ByteSizeLong(), 6000u);
  // The large one gave up rows; the small one, shorter all along, kept both.
  EXPECT_LT(s.message.sections(1).rows_size(), 10);
  EXPECT_TRUE(s.message.sections(1).truncated());
  EXPECT_EQ(s.message.sections(0).rows_size(), 2);
}

TEST(diagnostics_round, gives_up_whole_sections_when_the_snapshot_cannot_fit_without_rows) {
  PB::Diagnostics::DiagnosticsMessage m;
  add_section(m, "failures.recent", 2, "x");
  add_section(m, "second", 2, "x");
  add_section(m, "third", 2, "x");
  for (PB::Diagnostics::Section &s : *m.mutable_sections()) s.set_description(std::string(200, 'd'));
  dround::caps limits;
  // Room for one section's name, columns and description, not for two.
  limits.max_size = 400;
  const diagnostics_snapshot s = dround::assemble(m, limits, 1);
  EXPECT_LE(s.message.ByteSizeLong(), 400u);
  ASSERT_EQ(s.message.sections_size(), 1);
  // The first section - the core's own - is the last to go.
  EXPECT_EQ(s.message.sections(0).name(), "failures.recent");
  EXPECT_EQ(s.message.sections(0).rows_size(), 0);
  EXPECT_EQ(s.oversized, 2u);
  EXPECT_EQ(s.dropped, 2u);

  // A cap nothing fits leaves an empty snapshot rather than an oversized one.
  limits.max_size = 10;
  const diagnostics_snapshot none = dround::assemble(m, limits, 1);
  EXPECT_EQ(none.message.sections_size(), 0);
  EXPECT_EQ(none.oversized, 3u);

  // A snapshot that fits by giving up rows loses no section.
  limits.max_size = 100000;
  EXPECT_EQ(dround::assemble(m, limits, 1).oversized, 0u);
}

TEST(diagnostics_round, clamps_the_row_setting) {
  EXPECT_EQ(dround::clamp_rows(0), 1u);
  EXPECT_EQ(dround::clamp_rows(7), 7u);
  EXPECT_EQ(dround::clamp_rows(500), nscapi::diagnostics::max_rows);
}

TEST(diagnostics_snapshot_store, numbers_snapshots_and_measures_them) {
  nsclient::core::diagnostics_snapshot_store store;
  EXPECT_FALSE(store.latest());
  PB::Diagnostics::DiagnosticsMessage m;
  add_section(m, "a", 3);
  const nsclient::core::diagnostics_snapshot_ptr first = store.store(dround::assemble(m, dround::caps(), 5));
  const nsclient::core::diagnostics_snapshot_ptr second = store.store(dround::assemble(m, dround::caps(), 6));
  EXPECT_EQ(first->id + 1, second->id);
  EXPECT_EQ(second->wire_size, second->message.ByteSizeLong());
  EXPECT_EQ(store.latest(), second);
  store.reset();
  EXPECT_FALSE(store.latest());
}
