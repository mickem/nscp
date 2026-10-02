// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "perf_filter.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace {

PB::Common::PerformanceData make_numeric_perf(const std::string &alias, const double value, const double minimum, const double maximum) {
  PB::Common::PerformanceData perf;
  perf.set_alias(alias);
  PB::Common::PerformanceData::FloatValue *fv = perf.mutable_float_value();
  fv->set_value(value);
  fv->mutable_minimum()->set_value(minimum);
  fv->mutable_maximum()->set_value(maximum);
  return perf;
}

PB::Common::PerformanceData make_string_perf(const std::string &alias, const std::string &value) {
  PB::Common::PerformanceData perf;
  perf.set_alias(alias);
  perf.mutable_string_value()->set_value(value);
  return perf;
}

// Run the same filter pipeline render_perf uses over a set of perf records and
// return the rendered top line ("%(list)" of the matched records' keys).
std::string run_filter(const std::vector<std::string> &args, const std::vector<PB::Common::PerformanceData> &records) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("render_perf");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  modern_filter::data_container data;
  modern_filter::cli_helper<perf_filter::filter> filter_helper(request, &response, data);
  perf_filter::filter filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("%(list)", "%(key)", "%(key)", "EMPTY", "");
  if (!filter_helper.parse_options()) return "<parse error>";
  if (!filter_helper.build_filter(filter)) return "<build error>";
  for (const PB::Common::PerformanceData &p : records) {
    std::shared_ptr<perf_filter::filter_obj> record(new perf_filter::filter_obj(p));
    filter.match(record);
  }
  filter_helper.post_process(filter);
  std::string out;
  for (int i = 0; i < response.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += response.lines(i).message();
  }
  return out;
}

std::vector<PB::Common::PerformanceData> two_records() {
  // alpha: minimum=0,   maximum=100
  // bravo: minimum=100, maximum=200
  // min and max are distinct on both records, so a swapped accessor
  // matches the wrong record rather than accidentally the right one.
  return {make_numeric_perf("alpha", 50, 0, 100), make_numeric_perf("bravo", 150, 100, 200)};
}

}  // namespace

// ----------------------------------------------------------------------
// filter_obj accessors
// ----------------------------------------------------------------------

TEST(PerfFilterObj, ShowRendersAliasValueAndUnit) {
  PB::Common::PerformanceData perf = make_numeric_perf("alpha", 50, 0, 100);
  perf.mutable_float_value()->set_unit("B");
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.show(), "alpha=50B");
  EXPECT_EQ(obj.get_key(), "alpha");
  EXPECT_EQ(obj.get_unit(), "B");
}

TEST(PerfFilterObj, ShowWithoutUnitOrFloatValue) {
  const PB::Common::PerformanceData perf = make_string_perf("alpha", "some text");
  const perf_filter::filter_obj obj(perf);
  // String values have no unit, so show() is just alias=value.
  EXPECT_EQ(obj.get_unit(), "");
  EXPECT_EQ(obj.show(), "alpha=some text");
}

TEST(PerfFilterObj, ValueReadsStringValue) {
  const PB::Common::PerformanceData perf = make_string_perf("alpha", "some text");
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_value(), "some text");
}

TEST(PerfFilterObj, ValueEmptyWhenNoValueIsSet) {
  PB::Common::PerformanceData perf;
  perf.set_alias("alpha");
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_value(), "");
  EXPECT_EQ(obj.get_unit(), "");
  EXPECT_EQ(obj.show(), "alpha=");
}

// ----------------------------------------------------------------------
// warn/crit: prefer the original Nagios range syntax when present, fall
// back to the numeric lower bound, and stay empty otherwise (issue #748)
// ----------------------------------------------------------------------

TEST(PerfFilterObj, WarnPrefersOriginalRangeSyntax) {
  PB::Common::PerformanceData perf = make_numeric_perf("alpha", 50, 0, 100);
  perf.mutable_float_value()->set_warning_range("4:5");
  perf.mutable_float_value()->mutable_warning()->set_value(4);
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_warn(), "4:5");
}

TEST(PerfFilterObj, WarnFallsBackToNumericBound) {
  PB::Common::PerformanceData perf = make_numeric_perf("alpha", 50, 0, 100);
  perf.mutable_float_value()->mutable_warning()->set_value(4);
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_warn(), "4");
}

TEST(PerfFilterObj, WarnEmptyWhenUnset) {
  const PB::Common::PerformanceData perf = make_numeric_perf("alpha", 50, 0, 100);
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_warn(), "");
}

TEST(PerfFilterObj, WarnEmptyWithoutNumericValue) {
  const PB::Common::PerformanceData perf = make_string_perf("alpha", "some text");
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_warn(), "");
}

TEST(PerfFilterObj, CritPrefersOriginalRangeSyntax) {
  PB::Common::PerformanceData perf = make_numeric_perf("alpha", 50, 0, 100);
  perf.mutable_float_value()->set_critical_range("@0:90");
  perf.mutable_float_value()->mutable_critical()->set_value(0);
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_crit(), "@0:90");
}

TEST(PerfFilterObj, CritFallsBackToNumericBound) {
  PB::Common::PerformanceData perf = make_numeric_perf("alpha", 50, 0, 100);
  perf.mutable_float_value()->mutable_critical()->set_value(9);
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_crit(), "9");
}

TEST(PerfFilterObj, CritEmptyWhenUnset) {
  const PB::Common::PerformanceData perf = make_numeric_perf("alpha", 50, 0, 100);
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_crit(), "");
}

TEST(PerfFilterObj, CritEmptyWithoutNumericValue) {
  const PB::Common::PerformanceData perf = make_string_perf("alpha", "some text");
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_crit(), "");
}

TEST(PerfFilterObj, MaxReadsMaximumAndMinReadsMinimum) {
  const PB::Common::PerformanceData perf = make_numeric_perf("alpha", 50, 5, 100);
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_max(), "100");
  EXPECT_EQ(obj.get_min(), "5");
}

TEST(PerfFilterObj, MaxAndMinAreEmptyWithoutNumericValue) {
  const PB::Common::PerformanceData perf = make_string_perf("alpha", "some text");
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_max(), "");
  EXPECT_EQ(obj.get_min(), "");
}

TEST(PerfFilterObj, MaxAndMinAreEmptyWhenBoundsUnset) {
  PB::Common::PerformanceData perf;
  perf.set_alias("alpha");
  perf.mutable_float_value()->set_value(50);
  const perf_filter::filter_obj obj(perf);
  EXPECT_EQ(obj.get_max(), "");
  EXPECT_EQ(obj.get_min(), "");
}

// ----------------------------------------------------------------------
// keyword registration: `max` must read the maximum bound and `min` the
// minimum bound (they used to be registered against each other's accessor)
// ----------------------------------------------------------------------

TEST(PerfFilterKeywords, MaxKeywordMatchesMaximumBound) {
  const std::string msg = run_filter({"filter=max = '100'"}, two_records());
  EXPECT_NE(msg.find("alpha"), std::string::npos) << msg;
  EXPECT_EQ(msg.find("bravo"), std::string::npos) << msg;
}

TEST(PerfFilterKeywords, MaxKeywordDoesNotMatchMinimumBound) {
  // Nothing has maximum=0, but alpha's minimum is 0 — a swapped
  // accessor (max -> minimum) would match alpha here.
  const std::string msg = run_filter({"filter=max = '0'"}, two_records());
  EXPECT_EQ(msg.find("alpha"), std::string::npos) << msg;
  EXPECT_EQ(msg.find("bravo"), std::string::npos) << msg;
}

TEST(PerfFilterKeywords, MinKeywordMatchesMinimumBound) {
  const std::string msg = run_filter({"filter=min = '100'"}, two_records());
  EXPECT_NE(msg.find("bravo"), std::string::npos) << msg;
  EXPECT_EQ(msg.find("alpha"), std::string::npos) << msg;
}

TEST(PerfFilterKeywords, MinKeywordDoesNotMatchMaximumBound) {
  // Nothing has minimum=200; a swapped accessor (min -> maximum) would
  // match bravo here.
  const std::string msg = run_filter({"filter=min = '200'"}, two_records());
  EXPECT_EQ(msg.find("alpha"), std::string::npos) << msg;
  EXPECT_EQ(msg.find("bravo"), std::string::npos) << msg;
}

// ----------------------------------------------------------------------
// the neighbours: key/value/unit/warn/crit keep reading their own fields
// ----------------------------------------------------------------------

TEST(PerfFilterKeywords, ValueKeywordMatchesValue) {
  const std::string msg = run_filter({"filter=value = '150'"}, two_records());
  EXPECT_NE(msg.find("bravo"), std::string::npos) << msg;
  EXPECT_EQ(msg.find("alpha"), std::string::npos) << msg;
}

TEST(PerfFilterKeywords, KeyKeywordMatchesAlias) {
  const std::string msg = run_filter({"filter=key = 'alpha'"}, two_records());
  EXPECT_NE(msg.find("alpha"), std::string::npos) << msg;
  EXPECT_EQ(msg.find("bravo"), std::string::npos) << msg;
}

// ==============================================================
// filter_perf sort orders
// ==============================================================

namespace {
std::vector<std::string> sorted_keys(const std::vector<PB::Common::PerformanceData> &input, const bool reverse) {
  std::vector<PB::Common::PerformanceData> perfs = input;
  if (reverse)
    std::sort(perfs.begin(), perfs.end(), perf_filter::reverse_sort());
  else
    std::sort(perfs.begin(), perfs.end(), perf_filter::normal_sort());
  std::vector<std::string> keys;
  for (const PB::Common::PerformanceData &p : perfs) keys.push_back(p.alias());
  return keys;
}

// Numeric entries interleaved with string entries: the old comparators
// treated a string entry as "not less than" anything, so the numeric entries
// on either side of it were never compared with each other.
std::vector<PB::Common::PerformanceData> mixed_records() {
  return {make_numeric_perf("two", 2, 0, 10), make_string_perf("s1", "x"), make_numeric_perf("nine", 9, 0, 10), make_string_perf("s2", "y"),
          make_numeric_perf("five", 5, 0, 10), make_numeric_perf("one", 1, 0, 10)};
}
}  // namespace

TEST(PerfFilterSort, NormalOrdersNumericDescendingAndStringsLast) {
  const std::vector<std::string> keys = sorted_keys(mixed_records(), false);
  ASSERT_EQ(keys.size(), 6u);
  EXPECT_EQ(keys[0], "nine");
  EXPECT_EQ(keys[1], "five");
  EXPECT_EQ(keys[2], "two");
  EXPECT_EQ(keys[3], "one");
  EXPECT_FALSE(keys[4] == "one" || keys[4] == "two" || keys[4] == "five" || keys[4] == "nine");
  EXPECT_FALSE(keys[5] == "one" || keys[5] == "two" || keys[5] == "five" || keys[5] == "nine");
}

TEST(PerfFilterSort, ReverseOrdersNumericAscendingAndStringsLast) {
  const std::vector<std::string> keys = sorted_keys(mixed_records(), true);
  ASSERT_EQ(keys.size(), 6u);
  EXPECT_EQ(keys[0], "one");
  EXPECT_EQ(keys[1], "two");
  EXPECT_EQ(keys[2], "five");
  EXPECT_EQ(keys[3], "nine");
}

// The strict-weak-ordering axioms std::sort relies on: irreflexive,
// asymmetric, and consistent between numeric and non-numeric entries.
TEST(PerfFilterSort, ComparatorsAreStrictWeakOrderings) {
  const PB::Common::PerformanceData a = make_numeric_perf("a", 1, 0, 10);
  const PB::Common::PerformanceData b = make_numeric_perf("b", 2, 0, 10);
  const PB::Common::PerformanceData s = make_string_perf("s", "x");
  const PB::Common::PerformanceData t = make_string_perf("t", "y");
  perf_filter::normal_sort normal;
  perf_filter::reverse_sort reverse;

  EXPECT_FALSE(normal(a, a));
  EXPECT_FALSE(normal(s, s));
  EXPECT_FALSE(reverse(a, a));
  EXPECT_FALSE(reverse(s, s));

  EXPECT_TRUE(normal(b, a));
  EXPECT_FALSE(normal(a, b));
  EXPECT_TRUE(reverse(a, b));
  EXPECT_FALSE(reverse(b, a));

  // A numeric entry always precedes a string entry, never the other way.
  EXPECT_TRUE(normal(a, s));
  EXPECT_FALSE(normal(s, a));
  EXPECT_TRUE(reverse(a, s));
  EXPECT_FALSE(reverse(s, a));

  // String entries are equivalent to each other.
  EXPECT_FALSE(normal(s, t));
  EXPECT_FALSE(normal(t, s));
  EXPECT_FALSE(reverse(s, t));
  EXPECT_FALSE(reverse(t, s));
}
