// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "perf_filter.hpp"

#include <gtest/gtest.h>
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
