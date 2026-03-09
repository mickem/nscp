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

#include <gtest/gtest.h>

#include <parsers/cron/cron_parser.hpp>
#include <string>

// ======================================================================
// Helper
// ======================================================================

static std::string get_next(const std::string &schedule, const std::string &date) {
  const cron_parser::schedule s = cron_parser::parse(schedule);
  const boost::posix_time::ptime now(boost::posix_time::time_from_string(date));
  const boost::posix_time::ptime next = s.find_next(now);
  return boost::posix_time::to_iso_extended_string(next);
}

// ======================================================================
// parse() — round-trip via to_string()
// ======================================================================

TEST(CronParser, ParseSimpleValues) {
  EXPECT_EQ("0 0 1 1 0", cron_parser::parse("0 0 1 1 0").to_string());
  EXPECT_EQ("1 1 1 1 1", cron_parser::parse("1 1 1 1 1").to_string());
  EXPECT_EQ("2 3 4 5 6", cron_parser::parse("2 3 4 5 6").to_string());
  EXPECT_EQ("59 23 31 12 6", cron_parser::parse("59 23 31 12 6").to_string());
}

TEST(CronParser, ParseAllWildcards) { EXPECT_EQ("* * * * *", cron_parser::parse("* * * * *").to_string()); }

TEST(CronParser, ParseMixedWildcards) {
  EXPECT_EQ("* 23 31 12 6", cron_parser::parse("* 23 31 12 6").to_string());
  EXPECT_EQ("59 * 31 12 6", cron_parser::parse("59 * 31 12 6").to_string());
  EXPECT_EQ("59 23 * 12 6", cron_parser::parse("59 23 * 12 6").to_string());
  EXPECT_EQ("59 23 31 * 6", cron_parser::parse("59 23 31 * 6").to_string());
  EXPECT_EQ("59 23 31 12 *", cron_parser::parse("59 23 31 12 *").to_string());
}

TEST(CronParser, ParseCommaSeparatedLists) {
  EXPECT_EQ("10,11 23 31 12 6", cron_parser::parse("10,11 23 31 12 6").to_string());
  EXPECT_EQ("59 10,12,23 31 12 6", cron_parser::parse("59 10,12,23 31 12 6").to_string());
  EXPECT_EQ("0,15,30,45 * * * *", cron_parser::parse("0,15,30,45 * * * *").to_string());
}

TEST(CronParser, ParseBoundaryValues) {
  // min boundaries
  EXPECT_EQ("0 0 1 1 0", cron_parser::parse("0 0 1 1 0").to_string());
  // max boundaries
  EXPECT_EQ("59 23 31 12 6", cron_parser::parse("59 23 31 12 6").to_string());
}

// ======================================================================
// parse() — error cases
// ======================================================================

TEST(CronParser, ParseTooFewFields) {
  EXPECT_THROW(cron_parser::parse("0 0 1 1"), nsclient::nsclient_exception);
  EXPECT_THROW(cron_parser::parse("0 0 1"), nsclient::nsclient_exception);
  EXPECT_THROW(cron_parser::parse("0 0"), nsclient::nsclient_exception);
  EXPECT_THROW(cron_parser::parse("0"), nsclient::nsclient_exception);
  EXPECT_THROW(cron_parser::parse(""), nsclient::nsclient_exception);
}

TEST(CronParser, ParseTooManyFields) { EXPECT_THROW(cron_parser::parse("0 0 1 1 0 0"), nsclient::nsclient_exception); }

TEST(CronParser, ParseMinuteOutOfRange) {
  EXPECT_THROW(cron_parser::parse("60 0 1 1 0"), nsclient::nsclient_exception);
  EXPECT_THROW(cron_parser::parse("-1 0 1 1 0"), nsclient::nsclient_exception);
}

TEST(CronParser, ParseHourOutOfRange) { EXPECT_THROW(cron_parser::parse("0 24 1 1 0"), nsclient::nsclient_exception); }

TEST(CronParser, ParseDomOutOfRange) {
  EXPECT_THROW(cron_parser::parse("0 0 0 1 0"), nsclient::nsclient_exception);
  EXPECT_THROW(cron_parser::parse("0 0 32 1 0"), nsclient::nsclient_exception);
}

TEST(CronParser, ParseMonthOutOfRange) {
  EXPECT_THROW(cron_parser::parse("0 0 1 0 0"), nsclient::nsclient_exception);
  EXPECT_THROW(cron_parser::parse("0 0 1 13 0"), nsclient::nsclient_exception);
}

TEST(CronParser, ParseDowOutOfRange) { EXPECT_THROW(cron_parser::parse("0 0 1 1 7"), nsclient::nsclient_exception); }

TEST(CronParser, ParseInvalidTokens) {
  EXPECT_THROW(cron_parser::parse("a 0 1 1 0"), nsclient::nsclient_exception);
  EXPECT_THROW(cron_parser::parse("0 b 1 1 0"), nsclient::nsclient_exception);
}

// ======================================================================
// schedule_item — is_valid_for()
// ======================================================================

TEST(CronParser, IsValidForStar) {
  const auto item = cron_parser::schedule_item::parse("*", 0, 59);
  for (int i = 0; i <= 59; ++i) {
    EXPECT_TRUE(item.is_valid_for(i));
  }
}

TEST(CronParser, IsValidForSingleValue) {
  const auto item = cron_parser::schedule_item::parse("5", 0, 59);
  EXPECT_TRUE(item.is_valid_for(5));
  EXPECT_FALSE(item.is_valid_for(0));
  EXPECT_FALSE(item.is_valid_for(4));
  EXPECT_FALSE(item.is_valid_for(6));
  EXPECT_FALSE(item.is_valid_for(59));
}

TEST(CronParser, IsValidForList) {
  const auto item = cron_parser::schedule_item::parse("0,15,30,45", 0, 59);
  EXPECT_TRUE(item.is_valid_for(0));
  EXPECT_TRUE(item.is_valid_for(15));
  EXPECT_TRUE(item.is_valid_for(30));
  EXPECT_TRUE(item.is_valid_for(45));
  EXPECT_FALSE(item.is_valid_for(1));
  EXPECT_FALSE(item.is_valid_for(14));
  EXPECT_FALSE(item.is_valid_for(59));
}

// ======================================================================
// schedule_item — find_next()
// ======================================================================

TEST(CronParser, FindNextStarReturnsCurrentValue) {
  const auto item = cron_parser::schedule_item::parse("*", 0, 59);
  auto nv = item.find_next(0);
  EXPECT_EQ(0, nv.value);
  EXPECT_FALSE(nv.overflow);

  nv = item.find_next(30);
  EXPECT_EQ(30, nv.value);
  EXPECT_FALSE(nv.overflow);

  nv = item.find_next(59);
  EXPECT_EQ(59, nv.value);
  EXPECT_FALSE(nv.overflow);
}

TEST(CronParser, FindNextSingleNoOverflow) {
  const auto item = cron_parser::schedule_item::parse("30", 0, 59);
  auto nv = item.find_next(0);
  EXPECT_EQ(30, nv.value);
  EXPECT_FALSE(nv.overflow);

  nv = item.find_next(30);
  EXPECT_EQ(30, nv.value);
  EXPECT_FALSE(nv.overflow);
}

TEST(CronParser, FindNextSingleWithOverflow) {
  const auto item = cron_parser::schedule_item::parse("10", 0, 59);
  auto nv = item.find_next(11);
  EXPECT_EQ(10, nv.value);
  EXPECT_TRUE(nv.overflow);

  nv = item.find_next(59);
  EXPECT_EQ(10, nv.value);
  EXPECT_TRUE(nv.overflow);
}

TEST(CronParser, FindNextListPicksSmallestGe) {
  const auto item = cron_parser::schedule_item::parse("5,10,20", 0, 59);

  auto nv = item.find_next(0);
  EXPECT_EQ(5, nv.value);
  EXPECT_FALSE(nv.overflow);

  nv = item.find_next(5);
  EXPECT_EQ(5, nv.value);
  EXPECT_FALSE(nv.overflow);

  nv = item.find_next(6);
  EXPECT_EQ(10, nv.value);
  EXPECT_FALSE(nv.overflow);

  nv = item.find_next(11);
  EXPECT_EQ(20, nv.value);
  EXPECT_FALSE(nv.overflow);
}

TEST(CronParser, FindNextListOverflow) {
  const auto item = cron_parser::schedule_item::parse("5,10,20", 0, 59);

  auto nv = item.find_next(21);
  EXPECT_EQ(5, nv.value);
  EXPECT_TRUE(nv.overflow);

  nv = item.find_next(59);
  EXPECT_EQ(5, nv.value);
  EXPECT_TRUE(nv.overflow);
}

// ======================================================================
// schedule — is_valid_for()
// ======================================================================

TEST(CronParser, ScheduleIsValidForAllStars) {
  const auto s = cron_parser::parse("* * * * *");
  const boost::posix_time::ptime t(boost::posix_time::time_from_string("2016-06-15 14:30:00"));
  EXPECT_TRUE(s.is_valid_for(t));
}

TEST(CronParser, ScheduleIsValidForExactMatch) {
  // 2016-01-04 is a Monday (dow=1)
  const auto s = cron_parser::parse("30 14 4 1 1");
  const boost::posix_time::ptime t(boost::posix_time::time_from_string("2016-01-04 14:30:00"));
  EXPECT_TRUE(s.is_valid_for(t));
}

TEST(CronParser, ScheduleIsNotValidForMismatch) {
  const auto s = cron_parser::parse("30 14 4 1 1");
  // wrong minute
  const boost::posix_time::ptime t1(boost::posix_time::time_from_string("2016-01-04 14:31:00"));
  EXPECT_FALSE(s.is_valid_for(t1));
  // wrong hour
  const boost::posix_time::ptime t2(boost::posix_time::time_from_string("2016-01-04 15:30:00"));
  EXPECT_FALSE(s.is_valid_for(t2));
  // wrong day
  const boost::posix_time::ptime t3(boost::posix_time::time_from_string("2016-01-05 14:30:00"));
  EXPECT_FALSE(s.is_valid_for(t3));
}

// ======================================================================
// find_next() — minute field
// ======================================================================

TEST(CronParser, FindNextMinuteSingleWildcardRest) {
  EXPECT_EQ("2016-01-01T01:01:00", get_next("1 * * * *", "2016-01-01 01:00:00"));
  EXPECT_EQ("2016-01-01T02:01:00", get_next("1 * * * *", "2016-01-01 01:01:00"));
  EXPECT_EQ("2016-01-01T02:01:00", get_next("1 * * * *", "2016-01-01 01:02:00"));
}

TEST(CronParser, FindNextMinuteWrapsToNextHour) {
  EXPECT_EQ("2016-01-01T02:02:00", get_next("2 * * * *", "2016-01-01 01:02:00"));
  EXPECT_EQ("2016-01-01T02:02:00", get_next("2 * * * *", "2016-01-01 01:03:00"));
}

// ======================================================================
// find_next() — hour field
// ======================================================================

TEST(CronParser, FindNextHourSingleWildcardRest) {
  EXPECT_EQ("2016-01-01T01:00:00", get_next("* 1 * * *", "2016-01-01 00:00:00"));
  EXPECT_EQ("2016-01-01T01:01:00", get_next("* 1 * * *", "2016-01-01 01:00:00"));
  EXPECT_EQ("2016-01-02T01:00:00", get_next("* 1 * * *", "2016-01-01 01:59:00"));
  EXPECT_EQ("2016-01-02T01:00:00", get_next("* 1 * * *", "2016-01-01 02:00:00"));
}

TEST(CronParser, FindNextHourWrapsToNextDay) {
  EXPECT_EQ("2016-01-02T02:00:00", get_next("* 2 * * *", "2016-01-01 02:59:00"));
  EXPECT_EQ("2016-01-02T02:00:00", get_next("* 2 * * *", "2016-01-01 03:00:00"));
}

// ======================================================================
// find_next() — day-of-month field
// ======================================================================

TEST(CronParser, FindNextDomWildcardRest) {
  EXPECT_EQ("2016-01-01T00:01:00", get_next("* * 1 * *", "2016-01-01 00:00:00"));
  EXPECT_EQ("2016-01-01T23:59:00", get_next("* * 1 * *", "2016-01-01 23:58:00"));
  EXPECT_EQ("2016-02-01T00:00:00", get_next("* * 1 * *", "2016-01-01 23:59:00"));
  EXPECT_EQ("2016-02-01T00:00:00", get_next("* * 1 * *", "2016-01-02 02:00:00"));
}

TEST(CronParser, FindNextDomWrapsToNextMonth) {
  EXPECT_EQ("2016-02-02T00:00:00", get_next("* * 2 * *", "2016-01-02 23:59:00"));
  EXPECT_EQ("2016-02-02T00:00:00", get_next("* * 2 * *", "2016-01-03 02:00:00"));
}

// ======================================================================
// find_next() — month field
// ======================================================================

TEST(CronParser, FindNextMonthWildcardRest) {
  EXPECT_EQ("2016-01-01T00:01:00", get_next("* * * 1 *", "2016-01-01 00:00:00"));
  EXPECT_EQ("2016-01-31T23:59:00", get_next("* * * 1 *", "2016-01-31 23:58:00"));
  EXPECT_EQ("2017-01-01T00:00:00", get_next("* * * 1 *", "2016-01-31 23:59:00"));
  EXPECT_EQ("2017-01-01T00:00:00", get_next("* * * 1 *", "2016-02-01 02:00:00"));
  EXPECT_EQ("2017-01-01T00:00:00", get_next("* * * 1 *", "2016-09-07 23:18:14"));
}

TEST(CronParser, FindNextMonthFebruary) {
  EXPECT_EQ("2016-02-01T00:01:00", get_next("* * * 2 *", "2016-02-01 00:00:00"));
  EXPECT_EQ("2016-02-29T23:59:00", get_next("* * * 2 *", "2016-02-29 23:58:00"));
  EXPECT_EQ("2017-02-01T00:00:00", get_next("* * * 2 *", "2016-02-29 23:59:00"));
}

// ======================================================================
// find_next() — day-of-week field
// ======================================================================

TEST(CronParser, FindNextDowSunday) {
  // 2016-01-03 is a Sunday (dow=0)
  EXPECT_EQ("2016-01-03T00:01:00", get_next("* * * * 0", "2016-01-03 00:00:00"));
  // After 23:59 on Sunday, bumps to Monday 00:00, next Sunday is Jan 10
  EXPECT_EQ("2016-01-10T00:00:00", get_next("* * * * 0", "2016-01-03 23:59:00"));
}

TEST(CronParser, FindNextDowMonday) {
  // 2016-01-04 is a Monday (dow=1)
  EXPECT_EQ("2016-01-04T23:59:00", get_next("* * * * 1", "2016-01-04 23:58:00"));
  EXPECT_EQ("2016-01-11T00:00:00", get_next("* * * * 1", "2016-01-04 23:59:00"));
  EXPECT_EQ("2016-01-11T00:00:00", get_next("* * * * 1", "2016-01-05 00:00:00"));
}

TEST(CronParser, FindNextDowTuesday) {
  // 2016-01-05 is a Tuesday (dow=2)
  EXPECT_EQ("2016-01-05T23:59:00", get_next("* * * * 2", "2016-01-05 23:58:00"));
  EXPECT_EQ("2016-01-12T00:00:00", get_next("* * * * 2", "2016-01-05 23:59:00"));
}

// ======================================================================
// find_next() — comma-separated lists
// ======================================================================

TEST(CronParser, FindNextMinuteList) {
  EXPECT_EQ("2016-01-01T01:01:00", get_next("1,5 * * * *", "2016-01-01 01:00:00"));
  EXPECT_EQ("2016-01-01T01:05:00", get_next("1,5 * * * *", "2016-01-01 01:01:00"));
  EXPECT_EQ("2016-01-01T02:01:00", get_next("1,5 * * * *", "2016-01-01 01:05:00"));
}

TEST(CronParser, FindNextMinuteListMultipleValues) {
  EXPECT_EQ("2016-01-01T01:05:00", get_next("5,10 * * * *", "2016-01-01 01:00:00"));
  EXPECT_EQ("2016-01-01T01:10:00", get_next("5,10 * * * *", "2016-01-01 01:05:00"));
  EXPECT_EQ("2016-01-01T02:05:00", get_next("5,10 * * * *", "2016-01-01 01:10:00"));
}

TEST(CronParser, FindNextHourList) {
  EXPECT_EQ("2016-01-01T06:00:00", get_next("* 6,12,18 * * *", "2016-01-01 05:00:00"));
  EXPECT_EQ("2016-01-01T06:01:00", get_next("* 6,12,18 * * *", "2016-01-01 06:00:00"));
  EXPECT_EQ("2016-01-01T12:00:00", get_next("* 6,12,18 * * *", "2016-01-01 06:59:00"));
}

// ======================================================================
// find_next() — combined fields
// ======================================================================

TEST(CronParser, FindNextExactTimeNextDay) {
  // 30 14 => every day at 14:30
  EXPECT_EQ("2016-01-01T14:30:00", get_next("30 14 * * *", "2016-01-01 00:00:00"));
  // When now is exactly 14:30, bumps to 14:31; min(30) overflows and hour(14) overflows => next day
  EXPECT_EQ("2016-01-02T14:30:00", get_next("30 14 * * *", "2016-01-01 14:30:00"));
  EXPECT_EQ("2016-01-02T14:30:00", get_next("30 14 * * *", "2016-01-01 15:00:00"));
}

TEST(CronParser, FindNextMinuteAndHourCombination) {
  // every day at 00:00 — when now=00:00, bumps to 00:01; min(0) and hour(0) overflow => next day
  EXPECT_EQ("2016-01-02T00:00:00", get_next("0 0 * * *", "2016-01-01 00:00:00"));
  // when now=23:59, hour(0) overflows => next day at 00:00
  EXPECT_EQ("2016-01-02T00:00:00", get_next("0 0 * * *", "2016-01-01 23:59:00"));
}

TEST(CronParser, FindNextSpecificDateAndTime) {
  // 15th of every month at 12:00
  EXPECT_EQ("2016-01-15T12:00:00", get_next("0 12 15 * *", "2016-01-01 00:00:00"));
  // When now=12:00, bumps to 12:01; min(0) and hour(12) overflow => next 15th is Feb 15
  EXPECT_EQ("2016-02-15T12:00:00", get_next("0 12 15 * *", "2016-01-15 12:00:00"));
}

// ======================================================================
// find_next() — never runs "now"
// ======================================================================

TEST(CronParser, FindNextNeverRunsNow) {
  // When now_time exactly matches the schedule, it should advance by 1 minute
  // and then find the next occurrence.
  const auto s = cron_parser::parse("* * * * *");
  const boost::posix_time::ptime now(boost::posix_time::time_from_string("2016-06-15 10:00:00"));
  const boost::posix_time::ptime next = s.find_next(now);
  EXPECT_EQ("2016-06-15T10:01:00", boost::posix_time::to_iso_extended_string(next));
}

// ======================================================================
// schedule_item::to_string()
// ======================================================================

TEST(CronParser, ToStringStar) {
  const auto item = cron_parser::schedule_item::parse("*", 0, 59);
  EXPECT_EQ("*", item.to_string());
}

TEST(CronParser, ToStringSingle) {
  auto item = cron_parser::schedule_item::parse("42", 0, 59);
  EXPECT_EQ("42", item.to_string());
}

TEST(CronParser, ToStringList) {
  const auto item = cron_parser::schedule_item::parse("1,2,3", 0, 59);
  EXPECT_EQ("1,2,3", item.to_string());
}

// ======================================================================
// schedule::to_string() — full round-trip
// ======================================================================

TEST(CronParser, ScheduleToStringRoundTrip) {
  const std::vector<std::string> expressions = {"* * * * *", "0 0 1 1 0", "30 14 * * *", "0,15,30,45 * * * *", "59 23 31 12 6", "* 6,12,18 * * *"};
  for (const auto &expr : expressions) {
    EXPECT_EQ(expr, cron_parser::parse(expr).to_string());
  }
}

// ======================================================================
// Copy / assignment of schedule_item and next_value
// ======================================================================

TEST(CronParser, NextValueCopyAndAssign) {
  const cron_parser::next_value a(42, true);
  const cron_parser::next_value b(a);
  EXPECT_EQ(42, b.value);
  EXPECT_TRUE(b.overflow);

  cron_parser::next_value c(0, false);
  c = a;
  EXPECT_EQ(42, c.value);
  EXPECT_TRUE(c.overflow);
}

TEST(CronParser, ScheduleItemCopyAndAssign) {
  const auto item = cron_parser::schedule_item::parse("5,10", 0, 59);
  const cron_parser::schedule_item copy(item);
  EXPECT_EQ("5,10", copy.to_string());
  EXPECT_EQ(item.min_, copy.min_);
  EXPECT_EQ(item.max_, copy.max_);

  cron_parser::schedule_item assigned = item;
  EXPECT_EQ("5,10", assigned.to_string());
}

TEST(CronParser, ScheduleCopy) {
  auto s = cron_parser::parse("30 14 1 6 3");
  cron_parser::schedule copy(s);
  EXPECT_EQ("30 14 1 6 3", copy.to_string());
}
