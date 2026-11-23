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

// ============================================================================
// The pieces, one by one: every branch of the helpers the cases above only
// reach through expand() and normalize_arguments().
// ============================================================================

namespace {
bool ranged(const std::string &text) {
  detail::range r;
  return detail::parse_range(text, r);
}
std::string rendered(const unit u, const std::string &value) {
  std::string out, error;
  return detail::render_value(spec("k", above, u), value, out, error) ? out : "error: " + error;
}
bool can_compare(const unit u, const std::string &a, const std::string &b, double &na, double &nb) {
  return detail::comparable(spec("k", above, u), a, b, na, nb);
}
std::string filled(const spec_list &specs, const string_list &bounds, const std::string &alternative) {
  std::string out;
  return detail::fill_alternative(specs, bounds, alternative, out) ? out : "<dropped>";
}
}  // namespace

TEST(ThresholdPieces, Trim) {
  EXPECT_EQ("", detail::trim(""));
  EXPECT_EQ("", detail::trim(" \t "));
  EXPECT_EQ("a b", detail::trim(" \ta b\t "));
  EXPECT_EQ("a", detail::trim("a"));
}

TEST(ThresholdPieces, CharacterClasses) {
  EXPECT_EQ('a', detail::lower('A'));
  EXPECT_EQ('z', detail::lower('Z'));
  EXPECT_EQ('a', detail::lower('a'));
  EXPECT_EQ('@', detail::lower('@'));
  EXPECT_EQ('[', detail::lower('['));
  for (const char c : {'a', 'z', 'A', 'Z'}) EXPECT_TRUE(detail::is_alpha(c)) << c;
  for (const char c : {'`', '{', '@', '[', '0', '_'}) EXPECT_FALSE(detail::is_alpha(c)) << c;
  for (const char c : {'0', '9'}) EXPECT_TRUE(detail::is_digit(c)) << c;
  for (const char c : {'/', ':', 'a'}) EXPECT_FALSE(detail::is_digit(c)) << c;
  for (const char c : {'a', 'Z', '5', '_'}) EXPECT_TRUE(detail::is_word(c)) << c;
  for (const char c : {' ', '-', '(', '\''}) EXPECT_FALSE(detail::is_word(c)) << c;
}

TEST(ThresholdPieces, IsValue) {
  for (const std::string v : {"80", "80%", "-5", "+5", "1.5", ".5", "5.", "2d", "10GB", "500ms", "5Z", "5z", "-1.5h"}) EXPECT_TRUE(detail::is_value(v)) << v;
  for (const std::string v : {"", "-", "+", ".", "-.", "abc", "%", "80%%", "80%x", "80 %", "5[", "5{", "5_", "1.2.3", "1e5", "5m5"})
    EXPECT_FALSE(detail::is_value(v)) << v;
}

TEST(ThresholdPieces, ParseRange) {
  for (const std::string v : {"10", "@10", "10:", "@10:", "~:10", "@~:10", "10:20", "@10:20", "-5:5", "1k:1m"}) EXPECT_TRUE(ranged(v)) << v;
  for (const std::string v : {"", "@", ":", ":10", "~:", "@~:", "x:10", "10:x", "~", "10:20:30", "a"}) EXPECT_FALSE(ranged(v)) << v;
  detail::range r;
  ASSERT_TRUE(detail::parse_range("@~:10", r));
  EXPECT_TRUE(r.inside);
  EXPECT_TRUE(r.has_colon);
  EXPECT_EQ("~", r.start);
  EXPECT_EQ("10", r.end);
  detail::range bare;
  ASSERT_TRUE(detail::parse_range("10", bare));
  EXPECT_FALSE(bare.inside);
  EXPECT_FALSE(bare.has_colon);
  EXPECT_EQ("", bare.start);
  EXPECT_EQ("10", bare.end);
}

TEST(ThresholdPieces, SplitList) {
  EXPECT_EQ(string_list({""}), detail::split_list(""));
  EXPECT_EQ(string_list({"1"}), detail::split_list("1"));
  EXPECT_EQ(string_list({"1", "2", "3"}), detail::split_list("1, 2 ,3"));
  EXPECT_EQ(string_list({"", "2", ""}), detail::split_list(",2,"));
}

TEST(ThresholdPieces, RenderNumber) {
  EXPECT_EQ("0", detail::render_number(0));
  EXPECT_EQ("5", detail::render_number(5.0));
  EXPECT_EQ("-5", detail::render_number(-5.0));
  EXPECT_EQ("1.5", detail::render_number(1.5));
  EXPECT_EQ("-2.5", detail::render_number(-2.5));
  EXPECT_EQ("0.25", detail::render_number(0.25));
  EXPECT_EQ("1234567.8", detail::render_number(1234567.8));
  // As good as whole: no "5.000000".
  EXPECT_EQ("5", detail::render_number(5.0000000001));
  // Below the precision it is rendered with, the fraction is gone - and so is
  // the decimal point.
  EXPECT_EQ("0", detail::render_number(0.0000001));
  // Too large for the integer path, still not in scientific notation.
  EXPECT_EQ("2000000000000000", detail::render_number(2e15));
  EXPECT_EQ("999999999999999", detail::render_number(999999999999999.0));
}

TEST(ThresholdPieces, Durations) {
  for (const unit u : {unit_seconds_as_ms, unit_seconds, unit_milliseconds, unit_days}) EXPECT_TRUE(detail::is_duration(u));
  for (const unit u : {unit_plain, unit_percent, unit_percent_number}) EXPECT_FALSE(detail::is_duration(u));
  EXPECT_EQ(0.001, detail::bare_seconds_of(unit_milliseconds));
  EXPECT_EQ(86400.0, detail::bare_seconds_of(unit_days));
  EXPECT_EQ(1.0, detail::bare_seconds_of(unit_seconds));
  EXPECT_EQ(1.0, detail::bare_seconds_of(unit_seconds_as_ms));
  EXPECT_EQ(1.0, detail::bare_seconds_of(unit_plain));

  double seconds = -1;
  ASSERT_TRUE(detail::parse_duration("5", seconds));
  EXPECT_EQ(5.0, seconds);
  ASSERT_TRUE(detail::parse_duration("5", seconds, 86400.0));
  EXPECT_EQ(432000.0, seconds);
  ASSERT_TRUE(detail::parse_duration("5s", seconds));
  EXPECT_EQ(5.0, seconds);
  ASSERT_TRUE(detail::parse_duration("500ms", seconds));
  EXPECT_EQ(0.5, seconds);
  ASSERT_TRUE(detail::parse_duration("5m", seconds));
  EXPECT_EQ(300.0, seconds);
  ASSERT_TRUE(detail::parse_duration("1.5h", seconds));
  EXPECT_EQ(5400.0, seconds);
  ASSERT_TRUE(detail::parse_duration("2d", seconds));
  EXPECT_EQ(172800.0, seconds);
  ASSERT_TRUE(detail::parse_duration("1w", seconds));
  EXPECT_EQ(604800.0, seconds);
  ASSERT_TRUE(detail::parse_duration("-5m", seconds));
  EXPECT_EQ(-300.0, seconds);
  // Units are lower case, and a percentage or a size is not a duration.
  for (const std::string v : {"5x", "5S", "5MS", "5%", "5gb", "5min"}) EXPECT_FALSE(detail::parse_duration(v, seconds)) << v;
}

TEST(ThresholdPieces, SuffixAndNumber) {
  EXPECT_EQ("", detail::suffix_of("80"));
  EXPECT_EQ("", detail::suffix_of(""));
  EXPECT_EQ("%", detail::suffix_of("80%"));
  EXPECT_EQ("GB", detail::suffix_of("10GB"));
  EXPECT_EQ("ms", detail::suffix_of("-1.5ms"));
  EXPECT_EQ(80.0, detail::number_of("80"));
  EXPECT_EQ(80.0, detail::number_of("80%"));
  EXPECT_EQ(-1.5, detail::number_of("-1.5ms"));
  EXPECT_EQ(10.0, detail::number_of("10GB"));
}

TEST(ThresholdPieces, SizeSuffix) {
  for (const std::string v : {"b", "B", "k", "K", "m", "g", "t", "p", "e", "E", "kb", "KB", "mb", "Gb", "tB", "pb", "eb"}) EXPECT_TRUE(detail::is_size_suffix(v)) << v;
  for (const std::string v : {"", "x", "%", "d", "ms", "kx", "bb", "bk", "xb", "kbb", "gib"}) EXPECT_FALSE(detail::is_size_suffix(v)) << v;
}

TEST(ThresholdPieces, Comparable) {
  double a = 0, b = 0;
  // Durations are compared in seconds, whatever each end is written in.
  ASSERT_TRUE(can_compare(unit_seconds, "2d", "12h", a, b));
  EXPECT_EQ(172800.0, a);
  EXPECT_EQ(43200.0, b);
  ASSERT_TRUE(can_compare(unit_milliseconds, "100", "1s", a, b));
  EXPECT_EQ(0.1, a);
  EXPECT_EQ(1.0, b);
  ASSERT_TRUE(can_compare(unit_days, "2", "1w", a, b));
  EXPECT_EQ(172800.0, a);
  EXPECT_EQ(604800.0, b);
  EXPECT_FALSE(can_compare(unit_seconds, "5x", "5", a, b));
  EXPECT_FALSE(can_compare(unit_seconds, "5", "5x", a, b));

  // The same suffix, or none.
  ASSERT_TRUE(can_compare(unit_plain, "20", "10", a, b));
  EXPECT_EQ(20.0, a);
  EXPECT_EQ(10.0, b);
  ASSERT_TRUE(can_compare(unit_plain, "2D", "1d", a, b));
  EXPECT_EQ(2.0, a);
  EXPECT_EQ(1.0, b);
  // A plain keyword gives its suffixes their meaning itself.
  EXPECT_FALSE(can_compare(unit_plain, "2d", "12h", a, b));
  EXPECT_FALSE(can_compare(unit_plain, "2d", "12", a, b));
  EXPECT_FALSE(can_compare(unit_plain, "12", "2d", a, b));

  // A bare number of a percentage keyword is a percentage.
  ASSERT_TRUE(can_compare(unit_percent, "20", "10%", a, b));
  EXPECT_EQ(20.0, a);
  EXPECT_EQ(10.0, b);
  ASSERT_TRUE(can_compare(unit_percent, "20%", "10", a, b));
  ASSERT_TRUE(can_compare(unit_percent_number, "20", "10", a, b));
  // ... which is not a size.
  EXPECT_FALSE(can_compare(unit_percent, "20", "10g", a, b));
  EXPECT_FALSE(can_compare(unit_percent, "10g", "20%", a, b));

  // Sizes, a bare number next to one being bytes.
  ASSERT_TRUE(can_compare(unit_plain, "2000", "1k", a, b));
  EXPECT_EQ(2000.0, a);
  EXPECT_EQ(1024.0, b);
  ASSERT_TRUE(can_compare(unit_plain, "1k", "2000", a, b));
  EXPECT_EQ(1024.0, a);
  EXPECT_EQ(2000.0, b);
  ASSERT_TRUE(can_compare(unit_plain, "1MB", "1g", a, b));
  EXPECT_EQ(1048576.0, a);
  EXPECT_EQ(1073741824.0, b);
  ASSERT_TRUE(can_compare(unit_plain, "512b", "1kb", a, b));
  EXPECT_EQ(512.0, a);
  EXPECT_EQ(1024.0, b);
  ASSERT_TRUE(can_compare(unit_percent, "1g", "1t", a, b));
  EXPECT_FALSE(can_compare(unit_plain, "1k", "5x", a, b));
  EXPECT_FALSE(can_compare(unit_plain, "5x", "1k", a, b));
}

TEST(ThresholdPieces, RenderValue) {
  EXPECT_EQ("80", rendered(unit_plain, "80"));
  EXPECT_EQ("2d", rendered(unit_plain, "2d"));
  EXPECT_EQ("10G", rendered(unit_plain, "10G"));

  EXPECT_EQ("10%", rendered(unit_percent, "10"));
  EXPECT_EQ("10%", rendered(unit_percent, "10%"));
  EXPECT_EQ("10G", rendered(unit_percent, "10G"));
  EXPECT_EQ("error: Invalid unit for k: 10ms (expected a percentage, or a size such as 10G)", rendered(unit_percent, "10ms"));

  EXPECT_EQ("80", rendered(unit_percent_number, "80"));
  EXPECT_EQ("80", rendered(unit_percent_number, "80%"));
  EXPECT_EQ("error: Invalid unit for k: 80m (expected a percentage)", rendered(unit_percent_number, "80m"));

  EXPECT_EQ("2000", rendered(unit_seconds_as_ms, "2"));
  EXPECT_EQ("500", rendered(unit_seconds_as_ms, "0.5"));
  EXPECT_EQ("500", rendered(unit_seconds_as_ms, "500ms"));
  EXPECT_EQ("300000", rendered(unit_seconds_as_ms, "5m"));
  EXPECT_EQ("error: Invalid duration for k: 5x (expected a number of seconds, or a number with one of ms, s, m, h, d, w)", rendered(unit_seconds_as_ms, "5x"));

  EXPECT_EQ("2", rendered(unit_seconds, "2"));
  EXPECT_EQ("7200", rendered(unit_seconds, "2h"));
  EXPECT_EQ("0.5", rendered(unit_seconds, "500ms"));
  EXPECT_EQ("error: Invalid duration for k: 5% (expected a number of seconds, or a number with one of ms, s, m, h, d, w)", rendered(unit_seconds, "5%"));

  EXPECT_EQ("100", rendered(unit_milliseconds, "100"));
  EXPECT_EQ("1000", rendered(unit_milliseconds, "1s"));
  EXPECT_EQ("100", rendered(unit_milliseconds, "100ms"));
  EXPECT_EQ("error: Invalid duration for k: 5x (expected a number of milliseconds, or a number with one of ms, s, m, h, d, w)", rendered(unit_milliseconds, "5x"));

  EXPECT_EQ("3", rendered(unit_days, "3"));
  EXPECT_EQ("3", rendered(unit_days, "3d"));
  EXPECT_EQ("14", rendered(unit_days, "2w"));
  // A unit below a day is refused rather than rounded - each of them - and so
  // is one that is no duration at all.
  for (const std::string v : {"500ms", "30s", "30m", "12h", "5x", "5%"})
    EXPECT_EQ("error: Invalid duration for k: " + v + " (the keyword counts days: expected a number of days, or a number with d or w)", rendered(unit_days, v)) << v;
}

TEST(ThresholdPieces, ExpandRange) {
  const spec up("k", above), down("k", below);
  std::string out, error;
  const auto expanded = [&](const spec &s, const std::string &text) {
    out = error = "";
    return detail::expand_range(s, text, out, error) ? out : "error: " + error;
  };
  EXPECT_EQ("k > 10", expanded(up, "10"));
  EXPECT_EQ("k < 10", expanded(down, "10"));
  EXPECT_EQ("k >= 0 and k <= 10", expanded(up, "@10"));
  EXPECT_EQ("k < 10", expanded(up, "10:"));
  EXPECT_EQ("k > 10", expanded(down, "~:10"));
  EXPECT_EQ("k < 10 or k > 20", expanded(up, "10:20"));
  EXPECT_EQ("k >= 10 and k <= 20", expanded(up, "@10:20"));
  EXPECT_EQ("k <= 10", expanded(up, "@~:10"));
  EXPECT_EQ("k >= 10", expanded(up, "@10:"));
  EXPECT_EQ("k < 10 or k > 10", expanded(up, "10:10"));
  // Not a range at all.
  EXPECT_EQ("error: Invalid threshold: abc", expanded(up, "abc"));
  EXPECT_EQ("error: Invalid threshold: ~:", expanded(up, "~:"));
  EXPECT_EQ("error: Invalid threshold: ", expanded(up, ""));
  EXPECT_EQ("error: Invalid threshold: 20:10 (a range starts at its low end, as in 10:20)", expanded(up, "20:10"));
  // The unit is checked on both ends, whichever of them is wrong.
  const spec age("age", above, unit_seconds);
  EXPECT_EQ("error: Invalid duration for age: 5x (expected a number of seconds, or a number with one of ms, s, m, h, d, w)", expanded(age, "5x:10"));
  EXPECT_EQ("error: Invalid duration for age: 5x (expected a number of seconds, or a number with one of ms, s, m, h, d, w)", expanded(age, "10:5x"));
  EXPECT_EQ("error: Invalid duration for age: 5x (expected a number of seconds, or a number with one of ms, s, m, h, d, w)", expanded(age, "5x"));
  EXPECT_EQ("age < 60 or age > 3600", expanded(age, "1m:1h"));
  EXPECT_EQ("error: Invalid threshold: 1h:1m (a range starts at its low end, as in 1m:1h)", expanded(age, "1h:1m"));
  // Ends that cannot be compared here are passed on as they are.
  EXPECT_EQ("k < 2d or k > 12h", expanded(up, "2d:12h"));
}

TEST(ThresholdPieces, SplitAlternatives) {
  EXPECT_EQ(string_list({""}), detail::split_alternatives(""));
  EXPECT_EQ(string_list({"a = 1"}), detail::split_alternatives("a = 1"));
  EXPECT_EQ(string_list({"a = 1", "b = 2 and c = 3", "d = 4"}), detail::split_alternatives("a = 1 or b = 2 and c = 3 OR d = 4"));
  EXPECT_EQ(string_list({"a = 1", "b = 2"}), detail::split_alternatives("a = 1 Or b = 2"));
  EXPECT_EQ(string_list({"a = 1", "b = 2"}), detail::split_alternatives("a = 1 oR b = 2"));
  // Not inside quotes of either kind, nor inside parentheses.
  EXPECT_EQ(string_list({"a = 'x or y'", "b = 2"}), detail::split_alternatives("a = 'x or y' or b = 2"));
  EXPECT_EQ(string_list({"a = \"x or y\"", "b = 2"}), detail::split_alternatives("a = \"x or y\" or b = 2"));
  EXPECT_EQ(string_list({"a = \"it's or not\"", "b = 2"}), detail::split_alternatives("a = \"it's or not\" or b = 2"));
  EXPECT_EQ(string_list({"(a = 1 or (b = 2 or c = 3))", "d = 4"}), detail::split_alternatives("(a = 1 or (b = 2 or c = 3)) or d = 4"));
  // Only the word: not the letters in a keyword or a value.
  EXPECT_EQ(string_list({"error = 1", "order > 2", "color = 3"}), detail::split_alternatives("error = 1 or order > 2 or color = 3"));
  EXPECT_EQ(string_list({"or_else = 1"}), detail::split_alternatives("or_else = 1"));
  EXPECT_EQ(string_list({"a = 1 or_b"}), detail::split_alternatives("a = 1 or_b"));
  EXPECT_EQ(string_list({"a = 1 xor b"}), detail::split_alternatives("a = 1 xor b"));
  EXPECT_EQ(string_list({"a = 1 o"}), detail::split_alternatives("a = 1 o"));
  EXPECT_EQ(string_list({"a = 1 ox b"}), detail::split_alternatives("a = 1 ox b"));
  // At the very start it has nothing to its left; at the end, nothing to its
  // right.
  EXPECT_EQ(string_list({"or b = 2"}), detail::split_alternatives("or b = 2"));
  EXPECT_EQ(string_list({"a = 1", ""}), detail::split_alternatives("a = 1 or"));
  EXPECT_EQ(string_list({"(a)", "(b)"}), detail::split_alternatives("(a)or(b)"));
}

TEST(ThresholdPieces, FillAlternative) {
  spec_list specs;
  specs.push_back(spec("time", above));
  specs.push_back(spec("loss", above));
  const string_list bounds{"time > 5", ""};
  EXPECT_EQ("x = 1", filled(specs, bounds, "x = 1"));
  EXPECT_EQ("time > 5", filled(specs, bounds, "${time}"));
  EXPECT_EQ("x = 1 and time > 5 and y = 2", filled(specs, bounds, "x = 1 and ${time} and y = 2"));
  EXPECT_EQ("time > 5 and time > 5", filled(specs, bounds, "${time} and ${time}"));
  // A keyword without a value, or one the check does not have, drops it.
  EXPECT_EQ("<dropped>", filled(specs, bounds, "${loss}"));
  EXPECT_EQ("<dropped>", filled(specs, bounds, "${time} and ${loss}"));
  EXPECT_EQ("<dropped>", filled(specs, bounds, "${nope}"));
  EXPECT_EQ("<dropped>", filled(specs, bounds, "${}"));
  // An unfinished placeholder is text.
  EXPECT_EQ("x = '${time'", filled(specs, bounds, "x = '${time'"));
  EXPECT_EQ("time > 5 and ${loss", filled(specs, bounds, "${time} and ${loss"));
  // A range is two comparisons: alone it stands as it is, next to anything
  // else it is parenthesised - on either side.
  const string_list outside{"time < 1 or time > 5", ""}, inside{"time >= 1 and time <= 5", ""};
  EXPECT_EQ("time < 1 or time > 5", filled(specs, outside, "${time}"));
  EXPECT_EQ("guard = 1 and (time < 1 or time > 5)", filled(specs, outside, "guard = 1 and ${time}"));
  EXPECT_EQ("(time < 1 or time > 5) and guard = 1", filled(specs, outside, "${time} and guard = 1"));
  EXPECT_EQ("time >= 1 and time <= 5", filled(specs, inside, "${time}"));
  EXPECT_EQ("(time >= 1 and time <= 5) and guard = 1", filled(specs, inside, "${time} and guard = 1"));
}

TEST(ThresholdPieces, FillTemplate) {
  spec_list specs;
  specs.push_back(spec("time", above));
  specs.push_back(spec("loss", above));
  const string_list bounds{"time > 5", ""};
  EXPECT_EQ("time > 5 or x = 1", detail::fill_template(specs, bounds, "${time} or x = 1"));
  EXPECT_EQ("x = 1", detail::fill_template(specs, bounds, "${loss} or x = 1"));
  EXPECT_EQ("", detail::fill_template(specs, bounds, "${loss}"));
  EXPECT_EQ("", detail::fill_template(specs, bounds, ""));
  // An empty alternative is skipped rather than joined in.
  EXPECT_EQ("time > 5 or x = 1", detail::fill_template(specs, bounds, "${time} or or x = 1"));
  EXPECT_EQ("time > 5", detail::fill_template(specs, bounds, "${time} or"));
  EXPECT_EQ("${time} or ${loss}", detail::default_template(specs));
  EXPECT_EQ("", detail::default_template(spec_list()));
  EXPECT_EQ("${time}", detail::default_template(one("time", above)));
}

TEST(ThresholdPieces, ExpandReportsTheSideAndTheKeywords) {
  spec_list ping;
  ping.push_back(spec("time", above, unit_milliseconds, "60", "100"));
  ping.push_back(spec("loss", above, unit_percent, "5", "10"));
  string_list expressions(1, "stale");
  std::string error;
  for (const std::string value : {"", "  ", ",", ", ,"}) {
    EXPECT_FALSE(expand(ping, string_list(), critical, value, expressions, error)) << value;
    EXPECT_EQ("Missing value for -c (use none to remove the threshold)", error) << value;
    EXPECT_TRUE(expressions.empty()) << value;
    EXPECT_FALSE(expand(ping, string_list(), warning, value, expressions, error)) << value;
    EXPECT_EQ("Missing value for -w (use none to remove the threshold)", error) << value;
  }
  EXPECT_FALSE(expand(ping, string_list(), warning, "1,2,3", expressions, error));
  EXPECT_EQ("Too many values in threshold 1,2,3: expected at most 2 (time,loss)", error);
  EXPECT_FALSE(expand(one("load", above), string_list(), warning, "1,2", expressions, error));
  EXPECT_EQ("Too many values in threshold 1,2: expected at most 1 (load)", error);
  // The critical defaults fill in on the critical side.
  ASSERT_TRUE(expand(ping, string_list(), critical, ",20%", expressions, error)) << error;
  EXPECT_EQ(string_list({"time > 100 or loss > 20%"}), expressions);
  // A bad value in any position is reported, a bad default included.
  EXPECT_FALSE(expand(ping, string_list(), warning, "100,5x", expressions, error));
  EXPECT_EQ("Invalid unit for loss: 5x (expected a percentage, or a size such as 10G)", error);
  spec_list bad;
  bad.push_back(spec("age", above, unit_seconds, "5x", ""));
  bad.push_back(spec("size", above));
  EXPECT_FALSE(expand(bad, string_list(), warning, ",5", expressions, error));
  EXPECT_EQ("Invalid duration for age: 5x (expected a number of seconds, or a number with one of ms, s, m, h, d, w)", error);
  // ... but only where it is used: the critical side has no default for age.
  ASSERT_TRUE(expand(bad, string_list(), critical, ",5", expressions, error)) << error;
  EXPECT_EQ(string_list({"size > 5"}), expressions);
}

TEST(ThresholdPieces, ATemplateThatFillsToNothingAddsNoExpression) {
  // Two keywords without defaults and a template per keyword: the one whose
  // keyword was given no value contributes nothing, rather than an empty
  // expression.
  spec_list specs;
  specs.push_back(spec("cpu_pct", above));
  specs.push_back(spec("memory_pct", above));
  string_list templates;
  templates.push_back("${cpu_pct}");
  templates.push_back("${memory_pct}");
  EXPECT_EQ(string_list({"cpu_pct > 80"}), expand_with(specs, templates, warning, "80"));
  EXPECT_EQ(string_list({"memory_pct > 90"}), expand_with(specs, templates, warning, ",90"));
  EXPECT_EQ(string_list({"cpu_pct > 80", "memory_pct > 90"}), expand_with(specs, templates, warning, "80,90"));
  // And without templates, a keyword left out and without a default is left
  // out of the one expression.
  EXPECT_EQ(string_list({"memory_pct > 90"}), expand_with(specs, string_list(), critical, ",90"));
}

TEST(ThresholdPieces, Describe) {
  EXPECT_EQ("load > N", describe(one("load", above), string_list()));
  EXPECT_EQ("free < N%", describe(one("free", below, unit_percent), string_list()));
  EXPECT_EQ("N", describe_usage(one("load", above)));
  spec_list load;
  load.push_back(spec("load1", above));
  load.push_back(spec("load_5", below));
  load.push_back(spec("Load15", above, unit_seconds_as_ms));
  // The names are upper-cased, digits and all else left as they are.
  EXPECT_EQ("load1 > LOAD1 or load_5 < LOAD_5 or Load15 > LOAD15*1000", describe(load, string_list()));
  EXPECT_EQ("LOAD1,LOAD_5,LOAD15", describe_usage(load));
  EXPECT_EQ("", describe_usage(spec_list()));
  // One template per expression; one that names no keyword of the check is
  // left out.
  const spec_list count = one("count", above);
  string_list templates;
  templates.push_back("state = 'stopped'");
  templates.push_back("${nope}");
  templates.push_back("${count}");
  EXPECT_EQ("state = 'stopped'\" and \"count > N", describe(count, templates));
  EXPECT_EQ("", describe(count, string_list(1, "${nope}")));
}

TEST(ThresholdPieces, DefaultValue) {
  EXPECT_EQ("", default_value(spec_list(), warning));
  EXPECT_EQ("", default_value(one("load", above), warning));
  EXPECT_EQ("", default_value(one("load", above), critical));
  EXPECT_EQ("80", default_value(one("load", above, unit_plain, "80", ""), warning));
  EXPECT_EQ("", default_value(one("load", above, unit_plain, "80", ""), critical));
  spec_list specs;
  specs.push_back(spec("health", above, unit_plain, "", "0"));
  specs.push_back(spec("run_time", above, unit_plain, "80", "90"));
  specs.push_back(spec("other", above));
  // A keyword without a default keeps its place in the list.
  EXPECT_EQ(",80,", default_value(specs, warning));
  EXPECT_EQ("0,90,", default_value(specs, critical));
}

TEST(ThresholdPieces, DescribeUnitOfEveryUnit) {
  EXPECT_EQ("", describe_unit(spec("k", above, unit_plain)));
  EXPECT_EQ("", describe_unit(spec("k", above, unit_percent_number)));
  EXPECT_EQ("A number without a unit is a percentage; a size (10G) is used as written.", describe_unit(spec("k", above, unit_percent)));
  EXPECT_EQ("A number without a unit is in days; d and w are accepted as units (14d, 2w).", describe_unit(spec("k", above, unit_days)));
  EXPECT_EQ("A number without a unit is in milliseconds; ms, s, m, h, d and w are accepted as units (500ms, 2s).",
            describe_unit(spec("k", above, unit_milliseconds)));
  EXPECT_EQ("A number without a unit is in seconds; ms, s, m, h, d and w are accepted as units (500ms, 5m).", describe_unit(spec("k", above, unit_seconds)));
  EXPECT_EQ("A number without a unit is in seconds, which the keyword counts in milliseconds; ms, s, m, h, d and w are accepted as units (500ms, 5m).",
            describe_unit(spec("k", above, unit_seconds_as_ms)));
  // A value that is not one of the units reads as a plain keyword.
  EXPECT_EQ("", describe_unit(spec("k", above, static_cast<unit>(99))));
  EXPECT_EQ("80x", rendered(static_cast<unit>(99), "80x"));
}

TEST(ThresholdPieces, IsShortFlag) {
  char flag = 0;
  std::string attached = "stale";
  const auto parsed = [&](const std::string &arg) {
    flag = 0;
    attached = "stale";
    return is_short_flag(arg, flag, attached) ? std::string(1, flag) + "|" + attached : std::string("no");
  };
  EXPECT_EQ("w|", parsed("-w"));
  EXPECT_EQ("c|", parsed("-c"));
  EXPECT_EQ("w|", parsed("-w="));
  EXPECT_EQ("c|load > 5", parsed("-c=load > 5"));
  EXPECT_EQ("w|80", parsed("-w80"));
  EXPECT_EQ("w|.5", parsed("-w.5"));
  EXPECT_EQ("w|@10:20", parsed("-w@10:20"));
  EXPECT_EQ("c|~:10", parsed("-c~:10"));
  EXPECT_EQ("c|-5:", parsed("-c-5:"));
  EXPECT_EQ("c|+5", parsed("-c+5"));
  EXPECT_EQ("c|-.5", parsed("-c-.5"));
  for (const std::string arg : {"", "-", "w", "c", "x", "w=80", "-x", "-x5", "--w", "-warning", "-cores", "-w-", "-w+", "-c-foo", "-w-@1", "-w-~:1", "-w:", "-w "})
    EXPECT_EQ("no", parsed(arg)) << arg;
}

TEST(ThresholdPieces, IsKeyValue) {
  for (const std::string arg : {"a=b", "a=", "drive=C:", "show-all=true", "top_syntax1=x", "A=b", "a==b"}) EXPECT_TRUE(is_key_value(arg)) << arg;
  for (const std::string arg : {"", "a", "=b", "1a=b", "-a=b", "a b=c", "a = b", "used>80%", "a.b=c", "load"}) EXPECT_FALSE(is_key_value(arg)) << arg;
}

TEST(ThresholdPieces, IsFlagValue) {
  for (const std::string arg : {"80", "-5:", "-5", "~:10", "load = 5", "used>80%", "none", "", "-", "--", "-x"}) EXPECT_TRUE(is_flag_value(arg)) << arg;
  for (const std::string arg : {"-w", "-c", "-c90", "-w=1", "drive=/", "--drive", "--show-all=true"}) EXPECT_FALSE(is_flag_value(arg)) << arg;
}

TEST(ThresholdPieces, FindMissingValueInEveryShape) {
  EXPECT_EQ('w', find_missing_value({"w="}));
  EXPECT_EQ('w', find_missing_value({"--w="}));
  EXPECT_EQ('c', find_missing_value({"c="}));
  EXPECT_EQ('c', find_missing_value({"--c="}));
  // The first one found is the one reported.
  EXPECT_EQ('c', find_missing_value({"x=1", "c=", "w="}));
  EXPECT_EQ(0, find_missing_value({}));
  EXPECT_EQ(0, find_missing_value({"w", "c", "-w", "--w", "w=1", "xw=", "w= "}));
  EXPECT_EQ("Missing value for -c (use none to remove the threshold)", missing_value_error('c'));
}

TEST(ThresholdPieces, NormalizeArgumentsAtTheEdges) {
  EXPECT_EQ(args({}), normalize_arguments({}));
  // Nothing but a flag without a value: the shape defaults to key=value.
  EXPECT_EQ(args({"w="}), normalize_arguments({"-w"}));
  EXPECT_EQ(args({"w=", "c="}), normalize_arguments({"-w", "-c"}));
  EXPECT_EQ(args({"w=1", "c="}), normalize_arguments({"-w", "1", "-c"}));
  // The shape is that of the first argument that is not a short threshold,
  // also when the flag in front of it has no value.
  EXPECT_EQ(args({"--w=", "--drive", "c:"}), normalize_arguments({"-w", "--drive", "c:"}));
  EXPECT_EQ(args({"w=", "drive=c:"}), normalize_arguments({"-w", "drive=c:"}));
  EXPECT_EQ(args({"--w=5", "--c=", "--show-all"}), normalize_arguments({"-w5", "-c", "--show-all"}));
  // Dashed means two dashes and a name: these are not.
  EXPECT_EQ(args({"--", "w=5"}), normalize_arguments({"--", "-w", "5"}));
  EXPECT_EQ(args({"-x", "w=5"}), normalize_arguments({"-x", "-w", "5"}));
  EXPECT_EQ(args({"-ab", "w=5"}), normalize_arguments({"-ab", "-w", "5"}));
  EXPECT_EQ(args({"x", "w=5"}), normalize_arguments({"x", "-w", "5"}));
  EXPECT_EQ(args({"a--b", "w=5"}), normalize_arguments({"a--b", "-w", "5"}));
  // The value of a flag is not looked at for the shape.
  EXPECT_EQ(args({"w=--x"}), normalize_arguments({"-w=--x"}));
  // Later arguments do not change it either.
  EXPECT_EQ(args({"drive=c:", "w=5", "--show-all"}), normalize_arguments({"drive=c:", "-w", "5", "--show-all"}));
}
