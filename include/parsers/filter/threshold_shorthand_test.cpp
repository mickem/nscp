// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <parsers/filter/threshold_shorthand.hpp>

using namespace modern_filter::shorthand;

namespace {
typedef std::vector<std::string> string_list;
// The single expression a value expands to for a check without templates.
std::string expand_ok(const spec_list &specs, const std::string &value) {
  string_list expressions;
  std::string error;
  EXPECT_TRUE(expand(specs, string_list(), warning, value, expressions, error)) << value << ": " << error;
  EXPECT_LE(expressions.size(), 1u) << value;
  return expressions.empty() ? "" : expressions[0];
}
std::string expand_error(const spec_list &specs, const std::string &value) {
  string_list expressions;
  std::string error;
  EXPECT_FALSE(expand(specs, string_list(), warning, value, expressions, error)) << value;
  return error;
}
string_list expand_with(const spec_list &specs, const string_list &templates, const side which, const std::string &value) {
  string_list expressions;
  std::string error;
  EXPECT_TRUE(expand(specs, templates, which, value, expressions, error)) << value << ": " << error;
  return expressions;
}
spec_list one(const std::string &variable, const direction dir, const unit u = unit_plain, const std::string &w = "", const std::string &c = "") {
  return spec_list(1, spec(variable, dir, u, w, c));
}
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

// "20:10" would expand to "load < 20 or load > 10", which every value matches:
// an alert that never clears. It is refused instead.
TEST(ThresholdShorthand, AReversedRangeIsAnError) {
  const spec_list load = one("load", above);
  EXPECT_NE(std::string::npos, expand_error(load, "20:10").find("Invalid threshold: 20:10 (a range starts at its low end, as in 10:20)"));
  // Inside the range it would match nothing instead, which is as wrong.
  EXPECT_NE(std::string::npos, expand_error(load, "@20:10").find("Invalid threshold: @20:10"));
  EXPECT_NE(std::string::npos, expand_error(load, "5:-5").find("Invalid threshold"));
  EXPECT_NE(std::string::npos, expand_error(load, "1.5:1.25").find("Invalid threshold"));
  // Both ends on the same number is a range: everything but that number.
  EXPECT_EQ("load < 10 or load > 10", expand_ok(load, "10:10"));
  EXPECT_EQ("load < -5 or load > 5", expand_ok(load, "-5:5"));
  // In a list it is refused for the keyword it belongs to.
  spec_list ping;
  ping.push_back(spec("time", above));
  ping.push_back(spec("loss", above, unit_percent));
  EXPECT_NE(std::string::npos, expand_error(ping, "10:20,50:20").find("Invalid threshold: 50:20"));
}

TEST(ThresholdShorthand, AReversedRangeIsComparedInTheUnitOfTheKeyword) {
  // Durations by what they amount to, not by their digits.
  EXPECT_EQ("time < 500 or time > 2000", expand_ok(one("time", above, unit_seconds_as_ms), "500ms:2"));
  EXPECT_NE(std::string::npos, expand_error(one("time", above, unit_seconds_as_ms), "2:500ms").find("Invalid threshold"));
  EXPECT_NE(std::string::npos, expand_error(one("age", above, unit_seconds), "1h:30m").find("Invalid threshold"));
  // A bare number of a percentage keyword is a percentage.
  EXPECT_NE(std::string::npos, expand_error(one("free", below, unit_percent), "20:10%").find("Invalid threshold"));
  EXPECT_NE(std::string::npos, expand_error(one("load", above, unit_percent_number), "90%:80").find("Invalid threshold"));
  // Sizes by their scale.
  EXPECT_EQ("free < 500M or free > 1G", expand_ok(one("free", below, unit_percent), "500M:1G"));
  EXPECT_NE(std::string::npos, expand_error(one("free", below, unit_percent), "1G:500M").find("Invalid threshold"));
  EXPECT_NE(std::string::npos, expand_error(one("size", above), "2gb:1GB").find("Invalid threshold"));
  // What cannot be compared here is left to the filter: a plain keyword gives
  // its suffixes their meaning itself, and a percentage is not a size.
  EXPECT_EQ("age < 8d or age > 1w", expand_ok(one("age", above), "8d:1w"));
  EXPECT_EQ("free < 20% or free > 10G", expand_ok(one("free", below, unit_percent), "20:10G"));
}

// The expansion is read back by the filter parser, which takes "1234567.8" and
// not "1.23457e+06".
TEST(ThresholdShorthand, DurationsAreNeverRenderedInScientificNotation) {
  const spec_list ms = one("time", above, unit_seconds_as_ms);
  EXPECT_EQ("time > 1234567.8", expand_ok(ms, "1234.5678"));
  EXPECT_EQ("time > 0.5", expand_ok(ms, "0.0005"));
  EXPECT_EQ("time > 1.5", expand_ok(ms, "1.5ms"));
  EXPECT_EQ("time > 31536000000", expand_ok(ms, "365d"));
  EXPECT_EQ("age > 1234567.25", expand_ok(one("age", above, unit_seconds), "1234567.25"));
  EXPECT_EQ("age > 0.000123", expand_ok(one("age", above, unit_seconds), "0.000123"));
}

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
  EXPECT_NE(std::string::npos, expand_error(one("time", above, unit_milliseconds), "5x").find("Invalid duration for time: 5x"));
  EXPECT_NE(std::string::npos, expand_error(one("expires_in", below, unit_days), "5x").find("Invalid duration for expires_in: 5x"));
}

// check_ping: a bare number is milliseconds, as in the standard plugin, and
// "-w 1s" used to reach the filter as "time > 1s", which a millisecond keyword
// read as "time > 0".
TEST(ThresholdShorthand, MillisecondsUnitKeepsABareNumber) {
  const spec_list s = one("time", above, unit_milliseconds);
  EXPECT_EQ("time > 100", expand_ok(s, "100"));
  EXPECT_EQ("time > 100", expand_ok(s, "100ms"));
  EXPECT_EQ("time > 1000", expand_ok(s, "1s"));
  EXPECT_EQ("time > 1500", expand_ok(s, "1.5s"));
  EXPECT_EQ("time > 120000", expand_ok(s, "2m"));
  EXPECT_EQ("time > 0.5", expand_ok(s, "0.5"));
  EXPECT_EQ("time < 500 or time > 2000", expand_ok(s, "500:2s"));
  EXPECT_NE(std::string::npos, expand_error(s, "2s:500").find("Invalid threshold"));
}

// expires_in, signature_age: whole days.
TEST(ThresholdShorthand, DaysUnitTakesDaysAndWeeks) {
  const spec_list s = one("expires_in", below, unit_days);
  EXPECT_EQ("expires_in < 30", expand_ok(s, "30"));
  EXPECT_EQ("expires_in < 30", expand_ok(s, "30d"));
  EXPECT_EQ("expires_in < 14", expand_ok(s, "2w"));
  EXPECT_EQ("expires_in < 7 or expires_in > 60", expand_ok(s, "1w:60"));
  EXPECT_NE(std::string::npos, expand_error(s, "3w:10").find("Invalid threshold"));
  // A unit below a day would be a fraction the keyword never holds.
  EXPECT_NE(std::string::npos, expand_error(s, "12h").find("the keyword counts days"));
  EXPECT_NE(std::string::npos, expand_error(s, "30m").find("the keyword counts days"));
}

// check_uptime and the MSSQL ages count seconds and are declared so: a range
// written high to low in two different units is compared as durations and
// refused, where a plain keyword let "2d:12h" through as
// "uptime < 2d or uptime > 12h", which every uptime matches.
TEST(ThresholdShorthand, AReversedRangeInMixedDurationUnitsIsAnError) {
  const spec_list uptime = one("uptime", below, unit_seconds, "2d", "1d");
  EXPECT_NE(std::string::npos, expand_error(uptime, "2d:12h").find("Invalid threshold: 2d:12h"));
  EXPECT_EQ("uptime < 43200 or uptime > 172800", expand_ok(uptime, "12h:2d"));
  EXPECT_EQ("uptime < 172800", expand_ok(uptime, "2d"));
  const spec_list age = one("full_age", above, unit_seconds, "3d", "7d");
  EXPECT_NE(std::string::npos, expand_error(age, "1w:3d").find("Invalid threshold"));
  EXPECT_NE(std::string::npos, expand_error(age, "1h:1800").find("Invalid threshold"));
}

// A bare number next to a size is bytes, so the reversed range is seen.
TEST(ThresholdShorthand, ABareNumberNextToASizeIsBytes) {
  EXPECT_NE(std::string::npos, expand_error(one("size", above), "2000:1k").find("Invalid threshold"));
  EXPECT_EQ("size < 1000 or size > 1k", expand_ok(one("size", above), "1000:1k"));
}

// ---------------------------------------------------------------------------
// Lists.
// ---------------------------------------------------------------------------

TEST(ThresholdShorthand, AListBoundsOneKeywordPerValue) {
  spec_list s;
  s.push_back(spec("time", above));
  s.push_back(spec("loss", above, unit_percent));
  EXPECT_EQ("time > 100 or loss > 20%", expand_ok(s, "100,20%"));
  EXPECT_EQ("time > 100 or loss > 20%", expand_ok(s, "100, 20"));
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

// ---------------------------------------------------------------------------
// Templates: what the short form stands for when it is more than a bound.
// ---------------------------------------------------------------------------

TEST(ThresholdTemplate, TheConditionsNextToTheBoundStay) {
  const spec_list tcp = one("time", above, unit_seconds_as_ms, "1", "5");
  const string_list crit(1, "${time} or result != 'ok'");
  EXPECT_EQ(string_list({"time > 10000 or result != 'ok'"}), expand_with(tcp, crit, critical, "10"));
}

// A guard is part of the alternative it guards and is kept with it.
TEST(ThresholdTemplate, AGuardStaysOnItsBound) {
  const spec_list nps = one("reject_pct", above, unit_percent_number, "10", "25");
  EXPECT_EQ(string_list({"decisions >= min_requests and reject_pct > 20"}),
            expand_with(nps, string_list(1, "decisions >= min_requests and ${reject_pct}"), warning, "20"));
  EXPECT_EQ(string_list({"discarded > 0 or (decisions >= min_requests and reject_pct > 40) or (require_traffic = 1 and requests = 0)"}),
            expand_with(nps, string_list(1, "discarded > 0 or (decisions >= min_requests and ${reject_pct}) or (require_traffic = 1 and requests = 0)"),
                        critical, "40"));
  const spec_list restarts = one("restart_count", above, unit_plain, "3", "");
  EXPECT_EQ(string_list({"restart_count > 9 and started < 15m and started >= 0"}),
            expand_with(restarts, string_list(1, "${restart_count} and started < 15m and started >= 0"), warning, "9"));
}

// "full_age < 0" is the never-backed-up case, not the bound.
TEST(ThresholdTemplate, AnotherComparisonOnTheKeywordIsNotTheBound) {
  const spec_list backup = one("full_age", above, unit_plain, "3d", "7d");
  EXPECT_EQ(string_list({"full_age < 0 or full_age > 14d"}), expand_with(backup, string_list(1, "full_age < 0 or ${full_age}"), critical, "14d"));
}

TEST(ThresholdTemplate, AValueLeftOutOfAListKeepsItsDefault) {
  spec_list ping;
  ping.push_back(spec("time", above, unit_plain, "60", "100"));
  ping.push_back(spec("loss", above, unit_percent, "5", "10"));
  EXPECT_EQ(string_list({"time > 60 or loss > 20%"}), expand_with(ping, string_list(), warning, ",20%"));
  EXPECT_EQ(string_list({"time > 250 or loss > 10%"}), expand_with(ping, string_list(), critical, "250"));
  spec_list nscp;
  nscp.push_back(spec("crashes", above, unit_plain, "", "0"));
  nscp.push_back(spec("errors", above, unit_plain, "", "0"));
  EXPECT_EQ(string_list({"crashes > 5 or errors > 0"}), expand_with(nscp, string_list(), critical, "5"));
  // No default on the warning side: only what was given is bounded.
  EXPECT_EQ(string_list({"crashes > 5"}), expand_with(nscp, string_list(), warning, "5"));
}

// An alternative whose keyword has neither a value nor a default goes, with
// whatever guards it.
TEST(ThresholdTemplate, AnAlternativeWithoutAValueIsLeftOut) {
  spec_list s;
  s.push_back(spec("a", above));
  s.push_back(spec("b", above));
  const string_list t(1, "x = 1 and ${a} or ${b} and y = 2 or z = 3");
  EXPECT_EQ(string_list({"x = 1 and a > 1 or z = 3"}), expand_with(s, t, warning, "1"));
  EXPECT_EQ(string_list({"b > 2 and y = 2 or z = 3"}), expand_with(s, t, warning, ",2"));
}

TEST(ThresholdTemplate, ARangeIsParenthesisedNextToOtherConditions) {
  const spec_list tcp = one("time", above);
  // On its own between "or"s a range needs none: it is one more alternative.
  EXPECT_EQ(string_list({"time < 1 or time > 5 or result != 'ok'"}), expand_with(tcp, string_list(1, "${time} or result != 'ok'"), critical, "1:5"));
  EXPECT_EQ(string_list({"guard = 1 and (time >= 1 and time <= 5)"}), expand_with(tcp, string_list(1, "guard = 1 and ${time}"), critical, "@1:5"));
  EXPECT_EQ(string_list({"time < 1 or time > 5"}), expand_with(tcp, string_list(), critical, "1:5"));
}

// Several expressions on a side, as check_process has: each is its own
// template, and one with nothing to bound disappears.
TEST(ThresholdTemplate, EachTemplateIsItsOwnExpression) {
  const spec_list count = one("count", above);
  string_list crit;
  crit.push_back("state = 'stopped'");
  crit.push_back("count = 0");
  crit.push_back("${count}");
  EXPECT_EQ(string_list({"state = 'stopped'", "count = 0", "count < 1"}), expand_with(count, crit, critical, "1:"));
}

TEST(ThresholdTemplate, AnExpressionReplacesTheTemplates) {
  const spec_list tcp = one("time", above);
  EXPECT_EQ(string_list({"code = 500"}), expand_with(tcp, string_list(1, "${time} or result != 'ok'"), critical, "code = 500"));
}

TEST(ThresholdTemplate, AnEmptyValueIsAnError) {
  string_list expressions;
  std::string error;
  EXPECT_FALSE(expand(one("load", above), string_list(), warning, "", expressions, error));
  EXPECT_NE(std::string::npos, error.find("Missing value for -w")) << error;
  EXPECT_FALSE(expand(one("load", above), string_list(), critical, "  ", expressions, error));
  EXPECT_NE(std::string::npos, error.find("Missing value for -c")) << error;
}

TEST(ThresholdTemplate, APercentageTakesNoOtherUnit) {
  EXPECT_NE(std::string::npos, expand_error(one("load", above, unit_percent_number), "80m").find("Invalid unit for load: 80m"));
  EXPECT_NE(std::string::npos, expand_error(one("free", below, unit_percent), "10x").find("Invalid unit for free: 10x"));
  EXPECT_NE(std::string::npos, expand_error(one("free", below, unit_percent), "10ms").find("Invalid unit for free"));
  EXPECT_EQ("free < 10gb", expand_ok(one("free", below, unit_percent), "10gb"));
  EXPECT_EQ("free < 10M", expand_ok(one("free", below, unit_percent), "10M"));
}

TEST(ThresholdTemplate, DescribeAndDefaults) {
  const spec_list tcp = one("time", above, unit_seconds_as_ms, "1", "5");
  // N is in seconds and the keyword counts milliseconds: the help must not
  // claim "-w 2" is "time > 2".
  EXPECT_EQ("time > N*1000 or result != 'ok'", describe(tcp, string_list(1, "${time} or result != 'ok'")));
  EXPECT_EQ("time > N*1000", describe(tcp, string_list()));
  EXPECT_EQ("age > N", describe(one("age", above, unit_seconds), string_list()));
  // ... nor that a bare number is milliseconds: "-w 500" is 500 seconds here.
  EXPECT_EQ(0u, describe_unit(tcp.front()).find("A number without a unit is in seconds, which the keyword counts in milliseconds")) << describe_unit(tcp.front());
  EXPECT_EQ(0u, describe_unit(spec("time", above, unit_milliseconds)).find("A number without a unit is in milliseconds"));
  EXPECT_EQ(0u, describe_unit(spec("age", above, unit_seconds)).find("A number without a unit is in seconds;"));
  EXPECT_EQ(std::string::npos, describe_unit(spec("age", above, unit_seconds)).find("milliseconds"));
  EXPECT_EQ("N", describe_usage(tcp));
  EXPECT_EQ("1", default_value(tcp, warning));
  EXPECT_EQ("5", default_value(tcp, critical));
  spec_list ping;
  ping.push_back(spec("time", above, unit_plain, "60", "100"));
  ping.push_back(spec("loss", above, unit_percent, "5", "10"));
  EXPECT_EQ("time > TIME or loss > LOSS%", describe(ping, string_list()));
  EXPECT_EQ("TIME,LOSS", describe_usage(ping));
  EXPECT_EQ("60,5", default_value(ping, warning));
  EXPECT_EQ("", default_value(one("size", above), warning));
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
  EXPECT_EQ(args({"--w=80", "--drive", "c:"}), normalize_arguments({"-w", "80", "--drive", "c:"}));
}

TEST(ThresholdShorthandArguments, AmongDashedArgumentsItBecomesALongOption) {
  EXPECT_EQ(args({"--drive", "c:", "--w=80", "--c=90"}), normalize_arguments({"--drive", "c:", "-w", "80", "-c", "90"}));
  // A negative bound stays attached to its option, where it cannot be read as
  // an option itself.
  EXPECT_EQ(args({"--show-all", "--c=-5:"}), normalize_arguments({"--show-all", "-c", "-5:"}));
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

TEST(ThresholdShorthandArguments, ATrailingFlagHasNoValue) {
  EXPECT_EQ(args({"drive=c:", "w="}), normalize_arguments({"drive=c:", "-w"}));
  EXPECT_EQ(args({"--drive", "c:", "--w="}), normalize_arguments({"--drive", "c:", "-w"}));
}

// "-w -c 90": the -w was given nothing, and must not take "-c" for its value
// and leave a stray "90" behind.
TEST(ThresholdShorthandArguments, ABareFlagDoesNotTakeTheNextOptionForItsValue) {
  EXPECT_EQ(args({"w=", "c=90"}), normalize_arguments({"-w", "-c", "90"}));
  EXPECT_EQ(args({"w=", "c=90"}), normalize_arguments({"-w", "-c90"}));
  EXPECT_EQ(args({"--w=", "--show-all"}), normalize_arguments({"-w", "--show-all"}));
  // The shape is still that of the first real argument.
  EXPECT_EQ(args({"--w=", "--c=90", "--drive", "c:"}), normalize_arguments({"-w", "-c", "90", "--drive", "c:"}));
  // A negative bound is a value, not an option.
  EXPECT_EQ(args({"w=-5:", "c=-1"}), normalize_arguments({"-w", "-5:", "-c", "-1"}));
}

// "-w $ARG1$ drive=C:" with the macro unset: the drive must stay the drive.
TEST(ThresholdShorthandArguments, ABareFlagDoesNotTakeAKeyValueArgumentForItsValue) {
  EXPECT_EQ(args({"w=", "drive=/"}), normalize_arguments({"-w", "drive=/"}));
  EXPECT_EQ(args({"w=", "drive=C:", "c=90"}), normalize_arguments({"-w", "drive=C:", "-c", "90"}));
  EXPECT_EQ(args({"show-all=true", "c=", "max-depth=2", "w=", "empty_state=ok"}),
            normalize_arguments({"show-all=true", "-c", "max-depth=2", "-w", "empty_state=ok"}));
  // An expression is still a value: its "=" is set off, or is another operator.
  EXPECT_EQ(args({"w=state = 'ok'"}), normalize_arguments({"-w", "state = 'ok'"}));
  EXPECT_EQ(args({"w=used>80%", "c=load>=5"}), normalize_arguments({"-w", "used>80%", "-c", "load>=5"}));
  EXPECT_EQ(args({"w=load!=5"}), normalize_arguments({"-w", "load!=5"}));
  EXPECT_TRUE(is_key_value("drive=/"));
  EXPECT_TRUE(is_key_value("top-syntax=%(list)"));
  EXPECT_TRUE(is_key_value("w="));
  EXPECT_FALSE(is_key_value("=5"));
  EXPECT_FALSE(is_key_value("10:20"));
  EXPECT_FALSE(is_key_value("none"));
}

TEST(ThresholdShorthandArguments, AnAttachedNegativeValue) {
  EXPECT_EQ(args({"c=-5:", "w=-.5"}), normalize_arguments({"-c-5:", "-w-.5"}));
  EXPECT_EQ(args({"--show-all", "--c=-5:"}), normalize_arguments({"--show-all", "-c-5:"}));
  EXPECT_EQ(args({"c=+5"}), normalize_arguments({"-c+5"}));
  // A sign without a number after it is somebody else's argument.
  EXPECT_EQ(args({"-c-foo", "-w-"}), normalize_arguments({"-c-foo", "-w-"}));
}

TEST(ThresholdShorthandArguments, FindMissingValue) {
  EXPECT_EQ('w', find_missing_value({"drive=c:", "w=", "c=90"}));
  EXPECT_EQ('c', find_missing_value({"--drive", "c:", "--c="}));
  EXPECT_EQ(0, find_missing_value({"w=80", "c=90", "warning=", "--crit="}));
  EXPECT_EQ("Missing value for -w (use none to remove the threshold)", missing_value_error('w'));
}

// A list with every value left out is no threshold and no expression: it is
// what "-w $ARG1$,$ARG2$" leaves behind when neither macro is set.
TEST(ThresholdExpand, AnAllEmptyListIsAMissingValue) {
  spec_list ping;
  ping.push_back(spec("time", above, unit_milliseconds, "60", "100"));
  ping.push_back(spec("loss", above, unit_percent, "5", "10"));
  for (const std::string value : {",", ",,", " , "}) {
    std::vector<std::string> expressions;
    std::string error;
    EXPECT_FALSE(expand(ping, std::vector<std::string>(), warning, value, expressions, error)) << value;
    EXPECT_EQ(missing_value_error('w'), error) << value;
    EXPECT_TRUE(expressions.empty()) << value;
  }
  // One value is enough for it to be a list again.
  std::vector<std::string> expressions;
  std::string error;
  ASSERT_TRUE(expand(ping, std::vector<std::string>(), warning, ",20%", expressions, error)) << error;
  ASSERT_EQ(1u, expressions.size());
  EXPECT_EQ("time > 60 or loss > 20%", expressions[0]);
}
