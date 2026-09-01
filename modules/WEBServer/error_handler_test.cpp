// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The in-memory log buffer behind /api/v2/logs and the "last error" badge.
//
// Two things live here and neither was covered: the error tally the status
// endpoint reports, and the paging the log viewer scrolls with. The paging has
// three separate implementations (unfiltered, level-filtered, and "everything
// since index N") whose conventions do not agree - see the tests below, which
// pin what each one actually does.

#include "error_handler.hpp"

#include <gtest/gtest.h>

#include <list>
#include <string>
#include <vector>

namespace {

error_handler_interface::log_entry entry(const unsigned long index, const std::string &type, const std::string &message) {
  error_handler_interface::log_entry e;
  e.index = index;
  e.line = 1;
  e.type = type;
  e.file = "test.cpp";
  e.message = message;
  e.date = "2026-01-01 00:00:00";
  return e;
}

// Fills the handler with `count` entries indexed 0..count-1, alternating
// between "info" and "error" so the level filter has something to bite on.
void fill(error_handler &handler, const unsigned long count) {
  for (unsigned long i = 0; i < count; ++i) {
    const bool is_error = (i % 2) == 1;
    handler.add_message(is_error, entry(i, is_error ? "error" : "info", "message " + std::to_string(i)));
  }
}

std::vector<std::string> messages_of(const error_handler_interface::log_list &list) {
  std::vector<std::string> out;
  for (const auto &e : list) out.push_back(e.message);
  return out;
}

}  // namespace

// --- the error tally ---------------------------------------------------------

TEST(ErrorHandlerStatus, StartsClean) {
  error_handler handler;
  const error_handler::status status = handler.get_status();
  EXPECT_EQ(status.error_count, 0u);
  EXPECT_EQ(status.last_error, "");
}

TEST(ErrorHandlerStatus, OnlyErrorsAreCounted) {
  // Every log line is buffered, but the badge counts errors: a debug-level
  // daemon would otherwise show thousands of "errors".
  error_handler handler;
  handler.add_message(false, entry(0, "info", "hello"));
  handler.add_message(true, entry(1, "error", "boom"));
  handler.add_message(false, entry(2, "debug", "chatter"));

  const error_handler::status status = handler.get_status();
  EXPECT_EQ(status.error_count, 1u);
  EXPECT_EQ(status.last_error, "boom");

  std::size_t count = 0;
  EXPECT_EQ(handler.get_messages({}, 0, 10, count).size(), 3u);
}

TEST(ErrorHandlerStatus, TheLastErrorIsTheMostRecentOne) {
  error_handler handler;
  handler.add_message(true, entry(0, "error", "first"));
  handler.add_message(true, entry(1, "error", "second"));

  const error_handler::status status = handler.get_status();
  EXPECT_EQ(status.error_count, 2u);
  EXPECT_EQ(status.last_error, "second");
}

TEST(ErrorHandlerStatus, ANonErrorDoesNotClearTheLastError) {
  // The badge stays until it is explicitly reset; a later info line must not
  // quietly hide that something went wrong.
  error_handler handler;
  handler.add_message(true, entry(0, "error", "boom"));
  handler.add_message(false, entry(1, "info", "carrying on"));

  EXPECT_EQ(handler.get_status().last_error, "boom");
}

TEST(ErrorHandlerStatus, ResetClearsBothTheTallyAndTheBuffer) {
  error_handler handler;
  fill(handler, 4);
  handler.reset();

  const error_handler::status status = handler.get_status();
  EXPECT_EQ(status.error_count, 0u);
  EXPECT_EQ(status.last_error, "");

  std::size_t count = 1;
  EXPECT_TRUE(handler.get_messages({}, 0, 10, count).empty());
  EXPECT_EQ(count, 0u);
}

// --- unfiltered paging -------------------------------------------------------

TEST(ErrorHandlerPaging, UnfilteredPagingIsZeroBasedAndHalfOpen) {
  error_handler handler;
  fill(handler, 10);

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages({}, 2, 3, count);

  EXPECT_EQ(count, 10u);  // the total, so the viewer can size its scrollbar
  EXPECT_EQ(messages_of(page), (std::vector<std::string>{"message 2", "message 3", "message 4"}));
}

TEST(ErrorHandlerPaging, UnfilteredTheLastPageIsShortRatherThanPadded) {
  error_handler handler;
  fill(handler, 10);

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages({}, 8, 5, count);

  EXPECT_EQ(count, 10u);
  EXPECT_EQ(messages_of(page), (std::vector<std::string>{"message 8", "message 9"}));
}

TEST(ErrorHandlerPaging, UnfilteredAPositionPastTheEndIsEmptyRatherThanOutOfRange) {
  // The viewer can ask for a page that no longer exists (entries were reset
  // between requests); reading past the end of the buffer would be a crash.
  error_handler handler;
  fill(handler, 3);

  std::size_t count = 0;
  EXPECT_TRUE(handler.get_messages({}, 3, 10, count).empty());
  EXPECT_EQ(count, 3u);
  EXPECT_TRUE(handler.get_messages({}, 99, 10, count).empty());
  EXPECT_EQ(count, 3u);
}

TEST(ErrorHandlerPaging, UnfilteredAWholePageFitsExactly) {
  error_handler handler;
  fill(handler, 4);

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages({}, 0, 4, count);
  EXPECT_EQ(count, 4u);
  EXPECT_EQ(page.size(), 4u);
}

// --- level-filtered paging ---------------------------------------------------

TEST(ErrorHandlerPaging, TheLevelFilterKeepsOnlyTheRequestedTypes) {
  error_handler handler;
  fill(handler, 6);  // 0,2,4 = info; 1,3,5 = error

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages({"error"}, 0, 10, count);

  EXPECT_EQ(count, 3u);  // the number of *matching* entries, not the total
  EXPECT_EQ(messages_of(page), (std::vector<std::string>{"message 1", "message 3", "message 5"}));
}

TEST(ErrorHandlerPaging, SeveralLevelsCanBeRequestedAtOnce) {
  error_handler handler;
  handler.add_message(false, entry(0, "debug", "d"));
  handler.add_message(false, entry(1, "info", "i"));
  handler.add_message(true, entry(2, "error", "e"));

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages({"info", "error"}, 0, 10, count);

  EXPECT_EQ(count, 2u);
  EXPECT_EQ(messages_of(page), (std::vector<std::string>{"i", "e"}));
}

TEST(ErrorHandlerPaging, ALevelNothingMatchesYieldsAnEmptyPage) {
  error_handler handler;
  fill(handler, 4);

  std::size_t count = 1;
  EXPECT_TRUE(handler.get_messages({"critical"}, 0, 10, count).empty());
  EXPECT_EQ(count, 0u);
}

TEST(ErrorHandlerPaging, TheFilteredWindowIsOneBasedAndInclusiveUnlikeTheUnfilteredOne) {
  // Pinning the asymmetry rather than endorsing it: the unfiltered branch
  // slices [position, position+ipp), while this one keeps matching entries
  // whose 1-based ordinal falls in [position, position+ipp] - so it starts one
  // entry earlier and returns one more. Anything that changes this has to
  // change the log viewer's paging with it.
  error_handler handler;
  fill(handler, 12);  // six "error" entries: 1,3,5,7,9,11

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages({"error"}, 2, 2, count);

  EXPECT_EQ(count, 6u);
  // Ordinals 2,3,4 of the matching entries - three of them for ipp=2.
  EXPECT_EQ(messages_of(page), (std::vector<std::string>{"message 3", "message 5", "message 7"}));
}

// --- "everything since" paging ----------------------------------------------

TEST(ErrorHandlerSince, ReturnsOnlyEntriesStrictlyAfterTheGivenIndex) {
  // The log viewer polls with the highest index it has already rendered, so
  // that entry itself must not come back and be drawn twice.
  error_handler handler;
  fill(handler, 5);

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages_since(2, 0, 10, count);

  EXPECT_EQ(count, 2u);
  EXPECT_EQ(messages_of(page), (std::vector<std::string>{"message 3", "message 4"}));
}

TEST(ErrorHandlerSince, SinceZeroSkipsTheFirstEntry) {
  // A consequence of the strict comparison and of indices starting at 0: a
  // caller that means "everything" cannot spell it as since=0.
  error_handler handler;
  fill(handler, 3);

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages_since(0, 0, 10, count);

  EXPECT_EQ(count, 2u);
  EXPECT_EQ(messages_of(page), (std::vector<std::string>{"message 1", "message 2"}));
}

TEST(ErrorHandlerSince, NothingNewIsAnEmptyPage) {
  error_handler handler;
  fill(handler, 3);

  std::size_t count = 1;
  EXPECT_TRUE(handler.get_messages_since(99, 0, 10, count).empty());
  EXPECT_EQ(count, 0u);
}

TEST(ErrorHandlerSince, TheWindowFollowsTheSameOneBasedRuleAsTheLevelFilter) {
  error_handler handler;
  fill(handler, 10);

  std::size_t count = 0;
  const error_handler::log_list page = handler.get_messages_since(0, 2, 2, count);

  EXPECT_EQ(count, 9u);  // entries 1..9 are newer than index 0
  // Ordinals 2,3,4 of those nine.
  EXPECT_EQ(messages_of(page), (std::vector<std::string>{"message 2", "message 3", "message 4"}));
}
