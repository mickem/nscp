// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <parsers/filter/threshold_shorthand.hpp>

using namespace modern_filter::shorthand;

namespace {
std::string expand_ok(const spec_list &specs, const std::string &value) {
  std::string expression, error;
  EXPECT_TRUE(expand(specs, value, expression, error)) << value << ": " << error;
  return expression;
}
std::string expand_error(const spec_list &specs, const std::string &value) {
  std::string expression, error;
  EXPECT_FALSE(expand(specs, value, expression, error)) << value << " gave " << expression;
  return error;
}
spec_list one(const std::string &variable, const direction dir, const unit u = unit_plain) { return spec_list(1, spec(variable, dir, u)); }
typedef std::vector<std::string> args;
}  // namespace

// ---------------------------------------------------------------------------
// A bare number takes the side the check declared.
// ---------------------------------------------------------------------------

TEST(ThresholdShorthand, BareNumberUsesTheChecksDirection) {
  EXPECT_EQ("load > 80", expand_ok(one("load", above), "80"));
  EXPECT_EQ("charge < 20", expand_ok(one("charge", below), "20"));
  EXPECT_EQ("load > 1.5", expand_ok(one("load", above), "1.5"));
  EXPECT_EQ("load > 80", expand_ok(one("load", above), "  80 "));
}

TEST(ThresholdShorthand, PlainUnitHandsTheSuffixToTheFilter) {
  EXPECT_EQ("uptime < 2d", expand_ok(one("uptime", below), "2d"));
  EXPECT_EQ("used > 80%", expand_ok(one("used", above), "80%"));
  EXPECT_EQ("size > 10GB", expand_ok(one("size", above), "10GB"));
}

// ---------------------------------------------------------------------------
// Ranges follow the plugin guidelines whatever the check's direction is.
// ---------------------------------------------------------------------------

TEST(ThresholdShorthand, RangesFollowThePluginGuidelines) {
  const spec_list s = one("v", above);
  EXPECT_EQ("v < 10", expand_ok(s, "10:"));
  EXPECT_EQ("v > 10", expand_ok(s, "~:10"));
  EXPECT_EQ("v < 10 or v > 20", expand_ok(s, "10:20"));
  EXPECT_EQ("v >= 10 and v <= 20", expand_ok(s, "@10:20"));
  EXPECT_EQ("v >= 0 and v <= 10", expand_ok(s, "@10"));
  EXPECT_EQ("v >= 10", expand_ok(s, "@10:"));
  EXPECT_EQ("v <= 10", expand_ok(s, "@~:10"));
}

TEST(ThresholdShorthand, RangesIgnoreTheChecksDirection) {
  EXPECT_EQ("v < 10", expand_ok(one("v", below), "10:"));
  EXPECT_EQ("v > 10", expand_ok(one("v", below), "~:10"));
}

// ---------------------------------------------------------------------------
// Units.
// ---------------------------------------------------------------------------

TEST(ThresholdShorthand, PercentUnitMakesABareNumberAPercentage) {
  const spec_list s = one("free", below, unit_percent);
  EXPECT_EQ("free < 10%", expand_ok(s, "10"));
  EXPECT_EQ("free < 10%", expand_ok(s, "10%"));
  EXPECT_EQ("free < 10G", expand_ok(s, "10G"));
  EXPECT_EQ("free < 10% or free > 90%", expand_ok(s, "10:90"));
}

TEST(ThresholdShorthand, PercentNumberDropsThePercentSign) {
  const spec_list s = one("load", above, unit_percent_number);
  EXPECT_EQ("load > 80", expand_ok(s, "80"));
  EXPECT_EQ("load > 80", expand_ok(s, "80%"));
  EXPECT_EQ("load < 10 or load > 80", expand_ok(s, "10%:80%"));
}

TEST(ThresholdShorthand, SecondsAreRenderedAsMilliseconds) {
  const spec_list s = one("time", above, unit_seconds_as_ms);
  EXPECT_EQ("time > 5000", expand_ok(s, "5"));
  EXPECT_EQ("time > 5000", expand_ok(s, "5s"));
  EXPECT_EQ("time > 500", expand_ok(s, "500ms"));
  EXPECT_EQ("time > 500", expand_ok(s, "0.5"));
  EXPECT_EQ("time > 300000", expand_ok(s, "5m"));
  EXPECT_EQ("time > 3600000", expand_ok(s, "1h"));
  EXPECT_EQ("time < 1000 or time > 5000", expand_ok(s, "1:5"));
}

TEST(ThresholdShorthand, SecondsUnitConvertsTheSuffixes) {
  const spec_list s = one("age", above, unit_seconds);
  EXPECT_EQ("age > 30", expand_ok(s, "30"));
  EXPECT_EQ("age > 300", expand_ok(s, "5m"));
  EXPECT_EQ("age > 7200", expand_ok(s, "2h"));
  EXPECT_EQ("age > 172800", expand_ok(s, "2d"));
  EXPECT_EQ("age > 604800", expand_ok(s, "1w"));
  EXPECT_EQ("age > 0.5", expand_ok(s, "500ms"));
}

TEST(ThresholdShorthand, AnUnknownDurationUnitIsAnError) {
  EXPECT_NE(std::string::npos, expand_error(one("time", above, unit_seconds_as_ms), "5x").find("Invalid duration for time: 5x"));
  EXPECT_NE(std::string::npos, expand_error(one("time", above, unit_seconds), "5%").find("Invalid duration"));
}

// ---------------------------------------------------------------------------
// Lists.
// ---------------------------------------------------------------------------

TEST(ThresholdShorthand, AListBoundsOneKeywordPerValue) {
  spec_list s;
  s.push_back(spec("time", above));
  s.push_back(spec("loss", above, unit_percent));
  EXPECT_EQ("(time > 100) or (loss > 20%)", expand_ok(s, "100,20%"));
  EXPECT_EQ("(time > 100) or (loss > 20%)", expand_ok(s, "100, 20"));
  EXPECT_EQ("time > 100", expand_ok(s, "100"));
  EXPECT_EQ("loss > 20%", expand_ok(s, ",20"));
  EXPECT_EQ("time > 100", expand_ok(s, "100,"));
}

TEST(ThresholdShorthand, TooManyValuesIsAnError) {
  const std::string error = expand_error(one("load", above), "1,2");
  EXPECT_NE(std::string::npos, error.find("expected at most 1 (load)")) << error;
}

// ---------------------------------------------------------------------------
// Anything that is not a threshold is an expression and passes untouched.
// ---------------------------------------------------------------------------

TEST(ThresholdShorthand, AnExpressionPassesThrough) {
  const spec_list s = one("load", above);
  EXPECT_EQ("load > 80", expand_ok(s, "load > 80"));
  EXPECT_EQ("used > 80% or free < 1G", expand_ok(s, "used > 80% or free < 1G"));
  EXPECT_EQ("state = 'stopped'", expand_ok(s, "state = 'stopped'"));
  EXPECT_EQ("none", expand_ok(s, "none"));
  EXPECT_EQ("", expand_ok(s, ""));
}

TEST(ThresholdShorthand, IsThreshold) {
  EXPECT_TRUE(is_threshold("80"));
  EXPECT_TRUE(is_threshold("80%"));
  EXPECT_TRUE(is_threshold("-5"));
  EXPECT_TRUE(is_threshold("10:"));
  EXPECT_TRUE(is_threshold("~:10"));
  EXPECT_TRUE(is_threshold("@10:20"));
  EXPECT_TRUE(is_threshold("1,5,15"));
  EXPECT_TRUE(is_threshold(",5"));
  EXPECT_FALSE(is_threshold(""));
  EXPECT_FALSE(is_threshold(","));
  EXPECT_FALSE(is_threshold("~:"));
  EXPECT_FALSE(is_threshold(":"));
  EXPECT_FALSE(is_threshold(":10"));
  EXPECT_FALSE(is_threshold("load"));
  EXPECT_FALSE(is_threshold("load > 80"));
  EXPECT_FALSE(is_threshold("80 %"));
  EXPECT_FALSE(is_threshold("10:20:30"));
}

TEST(ThresholdShorthand, DescribeNamesTheKeywords) {
  EXPECT_EQ("load > N", describe(one("load", above)));
  EXPECT_EQ("free < N%", describe(one("free", below, unit_percent)));
  spec_list s;
  s.push_back(spec("time", above));
  s.push_back(spec("loss", above, unit_percent));
  EXPECT_EQ("time > N or loss > N%", describe(s));
}

// ---------------------------------------------------------------------------
// What is left of a default once its bound is replaced.
// ---------------------------------------------------------------------------

TEST(ThresholdShorthand, KeepUnboundedDropsOnlyTheBoundOnTheKeyword) {
  const std::vector<std::string> time(1, "time");
  EXPECT_EQ("result != 'ok'", keep_unbounded("time > 5000 or result != 'ok'", time));
  EXPECT_EQ("code < 200 or code >= 400 or result != 'ok'", keep_unbounded("code < 200 or code >= 400 or result != 'ok'", time));
  EXPECT_EQ("", keep_unbounded("time > 1000", time));
  EXPECT_EQ("", keep_unbounded("time>1000", time));
  EXPECT_EQ("", keep_unbounded("", time));
  EXPECT_EQ("responding = 0", keep_unbounded("responding = 0", time));
}

TEST(ThresholdShorthand, KeepUnboundedMatchesWholeKeywords) {
  const std::vector<std::string> time(1, "time");
  EXPECT_EQ("max_time > 5 or time_wait > 3", keep_unbounded("max_time > 5 or time_wait > 3 or time >= 2", time));
}

TEST(ThresholdShorthand, KeepUnboundedKeepsEqualityOnTheKeyword) {
  const std::vector<std::string> age(1, "checkdb_age");
  EXPECT_EQ("checkdb_age = -1", keep_unbounded("checkdb_age > 1209600 or checkdb_age = -1", age));
}

TEST(ThresholdShorthand, KeepUnboundedTreatsAnAndTermAsOneAlternative) {
  const std::vector<std::string> age(1, "transaction_age");
  EXPECT_EQ("", keep_unbounded("transaction_age > 1800 or is_idle = 1 and transaction_age > 300", age));
  const std::vector<std::string> restarts(1, "restarts");
  EXPECT_EQ("phase = 'Pending'", keep_unbounded("phase = 'Pending' or restarts > 5", restarts));
}

TEST(ThresholdShorthand, KeepUnboundedDoesNotSplitInsideQuotesOrParentheses) {
  const std::vector<std::string> v(1, "free_pct");
  EXPECT_EQ("name = 'this or that'", keep_unbounded("name = 'this or that' or free_pct < 10", v));
  EXPECT_EQ("health_status = 'Warning'", keep_unbounded("health_status = 'Warning' OR free_pct < 20", v));
  EXPECT_EQ("percent_disk_time > 80 or (has_device = 1 and health_status = 'Warning')",
            keep_unbounded("(has_space = 1 and free_pct < 20) or percent_disk_time > 80 or (has_device = 1 and health_status = 'Warning')", v));
}

TEST(ThresholdShorthand, KeepUnboundedTakesSeveralKeywords) {
  std::vector<std::string> v;
  v.push_back("free");
  v.push_back("used");
  EXPECT_EQ("", keep_unbounded("used > 80%", v));
  EXPECT_EQ("state = 'bad'", keep_unbounded("used > 80% or state = 'bad' or free < 1G", v));
}

// ---------------------------------------------------------------------------
// Rewriting the command line.
// ---------------------------------------------------------------------------

TEST(ThresholdShorthandArguments, AmongKeyValueArgumentsItBecomesKeyValue) {
  EXPECT_EQ(args({"drive=c:", "w=80", "c=90"}), normalize_arguments({"drive=c:", "-w", "80", "-c", "90"}));
}

TEST(ThresholdShorthandArguments, OnItsOwnItBecomesKeyValue) {
  EXPECT_EQ(args({"w=80", "c=90"}), normalize_arguments({"-w", "80", "-c", "90"}));
}

TEST(ThresholdShorthandArguments, LeadingShortFormDoesNotDecideTheShape) {
  EXPECT_EQ(args({"w=80", "drive=c:"}), normalize_arguments({"-w", "80", "drive=c:"}));
  EXPECT_EQ(args({"--w", "80", "--drive", "c:"}), normalize_arguments({"-w", "80", "--drive", "c:"}));
}

TEST(ThresholdShorthandArguments, AmongDashedArgumentsItBecomesALongOption) {
  EXPECT_EQ(args({"--drive", "c:", "--w", "80", "--c", "90"}), normalize_arguments({"--drive", "c:", "-w", "80", "-c", "90"}));
}

TEST(ThresholdShorthandArguments, AttachedValues) {
  EXPECT_EQ(args({"w=80", "c=90"}), normalize_arguments({"-w80", "-c=90"}));
  EXPECT_EQ(args({"w=10:", "c=@10:20"}), normalize_arguments({"-w10:", "-c@10:20"}));
  EXPECT_EQ(args({"w=~:10", "c=.5"}), normalize_arguments({"-w~:10", "-c.5"}));
}

TEST(ThresholdShorthandArguments, AnExpressionValueIsCarriedAlong) {
  EXPECT_EQ(args({"w=used > 80%"}), normalize_arguments({"-w", "used > 80%"}));
}

TEST(ThresholdShorthandArguments, OtherArgumentsAreLeftAlone) {
  EXPECT_EQ(args({"warning=load > 80", "crit=load > 90", "w=1", "c=2"}), normalize_arguments({"warning=load > 80", "crit=load > 90", "w=1", "c=2"}));
  EXPECT_EQ(args({"--warning", "load > 80", "--show-all"}), normalize_arguments({"--warning", "load > 80", "--show-all"}));
  // Starts with -w / -c but is not a short threshold.
  EXPECT_EQ(args({"-warning", "-cores"}), normalize_arguments({"-warning", "-cores"}));
  EXPECT_EQ(args(), normalize_arguments(args()));
}

TEST(ThresholdShorthandArguments, ATrailingFlagWithoutValueIsLeftForTheParser) {
  EXPECT_EQ(args({"drive=c:", "-w"}), normalize_arguments({"drive=c:", "-w"}));
}
