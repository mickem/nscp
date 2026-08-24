// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// End-to-end rendering test for the filter framework: the companion of
// filter_matrix_test.cpp for the *output* side. Where the matrix suite pins
// how expressions evaluate, this suite pins how the matched rows render into
// the message - through the same pipeline a real check uses (REST-style
// argument strings -> cli_helper -> modern_filter -> protobuf response) - both
// WITHOUT the number-format options (the legacy byte-for-byte rendering every
// unconfigured installation gets) and WITH them (`decimals`, `byte-unit`,
// `decimal-separator`, `thousands-separator`, issue #1428).
//
// The fixture mirrors what real checks register: a byte-valued keyword with a
// human string getter (check_drivesize's `used`), a percentage keyword
// (`used_pct`), plain float and int keywords in every magnitude and sign the
// framework meets (large, small, negative, zero), a signed byte count (the
// `rate` shape), an optional int with a no-value sentinel, strings that look
// like numbers, a duration keyword with a unit converter and an itos_as_time
// human getter (check_uptime's `uptime`), a date keyword rendered through
// format_date, a custom state keyword rendered by name (check_drivesize's
// `type`), and the shared format functions
// (format_bytes/format_number/convert_bytes). All values are exact binary
// fractions chosen so no assertion sits on a round-half tie.
//
// The three fixed contracts hold throughout: the options touch the *message*
// only (performance data keeps raw values and the '.' radix), threshold
// parsing is untouched, and an unconfigured check renders what it always did.

#include <gtest/gtest.h>

#include <boost/optional.hpp>
#include <cmath>
#include <ctime>
#include <map>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/format_functions.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace filter_rendering {

struct data_obj {
  std::string name;
  long long bytes;  // byte count: int keyword (type_size, perf) + human string getter
  double fraction;  // plain float keyword, no human getter (the template float path)
  long long count;  // plain int keyword (int template rendering is never formatted)
  double pct;       // percentage: float keyword + format_pct human getter
  long long big;    // large, negative and zero plain ints
  double bigf;      // floats large enough for the legacy scientific rendering
  double smallf;    // small, negative and sub-one floats
  long long delta;  // signed byte count (the check_drivesize `rate` shape)
  boost::optional<long long> oval;  // optional int with the "unknown" sentinel
  std::string sval;                 // strings, including number lookalikes
  long long dur;                    // duration in seconds: converter + itos_as_time human getter
  long long stamp;                  // epoch seconds: type_date + format_date human getter
  long long state;                  // custom state: int keyword rendered by name

  std::string show() const { return name; }
};

typedef std::shared_ptr<data_obj> data_ptr;
typedef parsers::where::filter_handler_impl<data_ptr> native_context;

// Converter for the duration keyword, the duration_keyword::parse_duration
// shape: thresholds written the way an operator thinks (`dur > 1h`) instead of
// in raw seconds.
static const parsers::where::value_type type_custom_duration = parsers::where::type_custom_int_2;
static parsers::where::node_type parse_duration(data_ptr /*object*/, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  const std::list<parsers::where::node_type> tokens = subject->get_list_value(context);
  if (tokens.size() == 2) {
    auto token = tokens.begin();
    const double count = (*token)->get_float_value(context);
    ++token;
    const std::string unit = (*token)->get_string_value(context);
    return parsers::where::factory::create_int(llround(count * static_cast<double>(str::format::time_unit_multiplier(unit))));
  }
  return parsers::where::factory::create_int(subject->get_int_value(context));
}

struct filter_obj_handler : native_context {
  filter_obj_handler() {
    // clang-format off
    registry_.add_string_var("name", [](data_ptr o) { return o->name; }, "Row name");
    registry_.add_int_var("bytes", parsers::where::type_size, [](data_ptr o) { return o->bytes; }, "Byte-count keyword (size units)").add_int_perf("B");
    registry_.add_float("fraction", [](data_ptr o) { return o->fraction; }, "Float keyword").add_float_perf();
    registry_.add_int_var("count", [](data_ptr o) { return o->count; }, "Plain int keyword").no_perf();
    registry_.add_float("pct", [](data_ptr o) { return o->pct; }, "Percentage keyword").no_perf();
    registry_.add_int_var("big", [](data_ptr o) { return o->big; }, "Large plain int keyword").no_perf();
    registry_.add_float("bigf", [](data_ptr o) { return o->bigf; }, "Large float keyword").no_perf();
    registry_.add_float("smallf", [](data_ptr o) { return o->smallf; }, "Small/negative float keyword").no_perf();
    registry_.add_int_var("delta", parsers::where::type_size, [](data_ptr o) { return o->delta; }, "Signed byte-count keyword").no_perf();
    registry_.add_optional_int_var("oval", [](data_ptr o) { return o->oval; }, "unknown", "Optional int keyword").no_perf();
    registry_.add_string_var("sval", [](data_ptr o) { return o->sval; }, "String keyword");
    registry_.add_int_var("dur", type_custom_duration, [](data_ptr o) { return o->dur; }, "Duration keyword (seconds)").no_perf();
    registry_.add_int_var("stamp", parsers::where::type_date, [](data_ptr o) { return o->stamp; }, "Date keyword (epoch seconds)").no_perf();
    registry_.add_int_var("state", [](data_ptr o) { return o->state; }, "State keyword (0/1)").no_perf();
    registry_.add_converter(type_custom_duration, &parse_duration);
    // clang-format on

    // The human renderings a template resolves for %(bytes) / %(pct) - the
    // same shape check_drivesize registers for %(used) / %(used_pct).
    registry_.add_human_string_context(
        "bytes", [](data_ptr o, parsers::where::evaluation_context context) { return str::format::format_byte_units(o->bytes, context->get_number_format()); },
        "");
    registry_.add_human_string_context(
        "pct", [](data_ptr o, parsers::where::evaluation_context context) { return str::format::format_pct(o->pct, context->get_number_format()); }, "");
    registry_.add_human_string_context(
        "delta", [](data_ptr o, parsers::where::evaluation_context context) { return str::format::format_byte_units(o->delta, context->get_number_format()); },
        "");

    // Renderings that are strings by nature and must stay outside the number
    // format: durations (check_uptime's `uptime`), dates (`boot`,
    // check_process_history's `first_seen`) and named states
    // (check_drivesize's `type`).
    registry_.add_human_string("dur", [](data_ptr o) { return str::format::itos_as_time(static_cast<unsigned long long>(o->dur) * 1000); }, "");
    registry_.add_human_string("stamp", [](data_ptr o) { return str::format::format_date(static_cast<std::time_t>(o->stamp)); }, "");
    registry_.add_human_string("state", [](data_ptr o) { return o->state ? std::string("started") : std::string("stopped"); }, "");

    parsers::where::format_functions::register_format_functions(registry_);
  }
};

typedef modern_filter::modern_filters<data_obj, filter_obj_handler> filter_type;

// The fixed data set. Every fraction is an exact binary number and no
// assertion below lands on a round-half tie, so the expected strings are the
// same on every platform:
//   name  bytes                fraction    count    pct    big            bigf         smallf     delta               oval     sval      dur     stamp       state
//   a     1536       (1.5KB)   1234.75     1234567  12.5   1234567890123  12345678.5   0.015625   -2684354560 (-2.5GB) 512      "1234.5"  273600  1756022400  1
//   b     2684354560 (2.5GB)   0.25        42       7.25   -987654321     -9876543.25  -0.015625  -1536       (-1.5KB) (none)   "up"      45      946684800   0
//   c     734003200  (700MB)   2.7109375   0        99.75  0              1000000      0.5        1024        (1KB)    98304    "2.5GB"   7200    1234567890  1
std::vector<data_obj> rows() {
  return {
      {"a", 1536, 1234.75, 1234567, 12.5, 1234567890123LL, 12345678.5, 0.015625, -2684354560LL, boost::optional<long long>(512), "1234.5", 273600, 1756022400LL,
       1},
      {"b", 2684354560LL, 0.25, 42, 7.25, -987654321LL, -9876543.25, -0.015625, -1536, boost::none, "up", 45, 946684800LL, 0},
      {"c", 734003200LL, 2.7109375, 0, 99.75, 0, 1000000.0, 0.5, 1024, boost::optional<long long>(98304), "2.5GB", 7200, 1234567890LL, 1},
  };
}

struct run_result {
  PB::Common::ResultCode code;
  std::string message;
  std::vector<PB::Common::PerformanceData> perf;
  bool built;  // false when parse/build/validate failed (message carries why)
};

// Run one query through the exact pipeline a real check uses, including the
// has_errors() probe after each match that turns a render error (a bad unit in
// format_bytes, say) into the UNKNOWN a real check returns.
run_result run_query(const std::vector<std::string> &args, const std::string &detail_syntax, const std::string &top_syntax = "%(list)",
                     const std::string &perf_syntax = "%(name)") {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("filter_rendering");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, &response, data);
  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax(top_syntax, detail_syntax, perf_syntax, "EMPTY", "");

  run_result out;
  out.built = false;
  out.code = PB::Common::ResultCode::UNKNOWN;
  if (!filter_helper.parse_options()) {
    out.message = response.lines_size() > 0 ? response.lines(0).message() : "<parse error>";
    out.code = response.result();
    return out;
  }
  if (!filter_helper.build_filter(filter)) {
    out.message = response.lines_size() > 0 ? response.lines(0).message() : "<build error>";
    out.code = response.result();
    return out;
  }
  for (const data_obj &r : rows()) {
    data_ptr record(new data_obj(r));
    filter.match(record);
    if (filter.has_errors()) {
      nscapi::protobuf::functions::set_response_bad(response, "Filter processing failed: " + filter.get_errors());
      out.message = response.lines_size() > 0 ? response.lines(0).message() : "";
      out.code = response.result();
      return out;
    }
  }
  filter_helper.post_process(filter);
  out.built = true;
  out.code = response.result();
  out.message = response.lines_size() > 0 ? response.lines(0).message() : "";
  for (int i = 0; i < response.lines(0).perf_size(); ++i) out.perf.push_back(response.lines(0).perf(i));
  return out;
}

// Render `detail_syntax` for every row (filter=none matches all) with the
// given extra arguments and return the %(list) message.
std::string render_all(const std::string &detail_syntax, const std::vector<std::string> &extra_args = {}) {
  std::vector<std::string> args = {"filter=none", "warning=none", "critical=none"};
  args.insert(args.end(), extra_args.begin(), extra_args.end());
  const run_result r = run_query(args, detail_syntax);
  EXPECT_TRUE(r.built) << r.message;
  EXPECT_EQ(r.code, PB::Common::ResultCode::OK) << r.message;
  return r.message;
}

const PB::Common::PerformanceData *find_perf(const run_result &r, const std::string &alias) {
  for (const PB::Common::PerformanceData &p : r.perf) {
    if (p.alias() == alias) return &p;
  }
  return nullptr;
}

}  // namespace filter_rendering

using namespace filter_rendering;

// ============================================================================
// Default rendering (no formatting options): the legacy byte-for-byte output
// an unconfigured check has always produced.
// ============================================================================

TEST(FilterRenderingDefault, HumanByteKeywordAutoScalesPerValue) {
  // Each value picks its own unit - the very mix the byte-unit option exists
  // to pin down (issue #1428's "140.293GB/0.983TB").
  EXPECT_EQ(render_all("%(bytes)"), "1.5KB, 2.5GB, 700MB");
}

TEST(FilterRenderingDefault, FloatKeywordRendersWithStreamPrecision) {
  // No human getter and no options: floats take the legacy ostream rendering
  // (6 significant digits), hence 2.7109375 -> "2.71094".
  EXPECT_EQ(render_all("%(fraction)"), "1234.75, 0.25, 2.71094");
}

TEST(FilterRenderingDefault, IntKeywordRendersPlain) { EXPECT_EQ(render_all("%(count)"), "1234567, 42, 0"); }

TEST(FilterRenderingDefault, PctHumanKeywordRendersTwoDecimals) {
  // Percentages have always rendered with two fixed decimals.
  EXPECT_EQ(render_all("%(pct)"), "12.50, 7.25, 99.75");
}

TEST(FilterRenderingDefault, FormatBytesAutoScalesAndAppendsTheUnit) { EXPECT_EQ(render_all("%(format_bytes(bytes))"), "1.5KB, 2.5GB, 700MB"); }

TEST(FilterRenderingDefault, FormatBytesPinnedUnitLeavesTheUnitOut) {
  // With an explicit unit the syntax string spells the unit out, so the
  // function does not append it. Up to three decimals, trailing zeros
  // stripped: 1536B in MB is 0.00146... -> "0.001".
  EXPECT_EQ(render_all("%(format_bytes(bytes, 'MB'))"), "0.001, 2560, 700");
}

TEST(FilterRenderingDefault, FormatBytesUnitIsCaseInsensitive) {
  // format_bytes(v, 'gb') used to render nonsense (1.27e-10) because the unit
  // table was compared case sensitively (#1428).
  EXPECT_EQ(render_all("%(format_bytes(bytes, 'mb'))"), render_all("%(format_bytes(bytes, 'MB'))"));
}

TEST(FilterRenderingDefault, FormatBytesExplicitDecimalsArgument) { EXPECT_EQ(render_all("%(format_bytes(bytes, 'GB', 2))"), "0.00, 2.50, 0.68"); }

TEST(FilterRenderingDefault, FormatNumberDefaultsToUpToThreeDecimals) { EXPECT_EQ(render_all("%(format_number(fraction))"), "1234.75, 0.25, 2.711"); }

TEST(FilterRenderingDefault, FormatNumberExplicitDecimalsArgument) { EXPECT_EQ(render_all("%(format_number(fraction, 2))"), "1234.75, 0.25, 2.71"); }

TEST(FilterRenderingDefault, ExplicitDefaultsKeepTheLegacyRendering) {
  // Spelling the defaults out (`decimals=-1`, `decimal-separator=.`) must not
  // flip the message into the reformatted rendering: floats keep the stream
  // precision, not the up-to-three-decimals form.
  EXPECT_EQ(render_all("%(fraction)", {"decimals=-1", "decimal-separator=."}), "1234.75, 0.25, 2.71094");
}

TEST(FilterRenderingDefault, PerformanceDataCarriesTheRawValues) {
  const run_result r = run_query({"warning=bytes > 1k", "critical=none"}, "%(name)");
  ASSERT_TRUE(r.built) << r.message;
  const PB::Common::PerformanceData *p = find_perf(r, "a");
  ASSERT_NE(p, nullptr) << r.message;
  ASSERT_TRUE(p->has_float_value());
  EXPECT_EQ(p->float_value().unit(), "B");
  EXPECT_DOUBLE_EQ(p->float_value().value(), 1536.0);
  ASSERT_TRUE(p->float_value().has_warning());
  EXPECT_DOUBLE_EQ(p->float_value().warning().value(), 1024.0);
}

TEST(FilterRenderingDefault, LargeAndNegativeIntsRenderEveryDigit) {
  // xtos_non_sci: a 13-digit int renders all its digits, never scientific.
  EXPECT_EQ(render_all("%(big)"), "1234567890123, -987654321, 0");
}

TEST(FilterRenderingDefault, LargeFloatsRenderScientificByLegacy) {
  // The legacy 6-significant-digit stream rendering goes scientific past a
  // million - pinned deliberately: this is what an unconfigured check shows,
  // and what the decimals option exists to escape.
  EXPECT_EQ(render_all("%(bigf)"), "1.23457e+07, -9.87654e+06, 1e+06");
}

TEST(FilterRenderingDefault, SmallAndNegativeFloatsRenderPlain) { EXPECT_EQ(render_all("%(smallf)"), "0.015625, -0.015625, 0.5"); }

TEST(FilterRenderingDefault, SignedByteKeywordAutoScalesWithItsSign) { EXPECT_EQ(render_all("%(delta)"), "-2.5GB, -1.5KB, 1KB"); }

TEST(FilterRenderingDefault, OptionalKeywordRendersValueOrSentinel) { EXPECT_EQ(render_all("%(oval)"), "512, unknown, 98304"); }

TEST(FilterRenderingDefault, StringKeywordRendersVerbatim) {
  // Number and byte lookalikes included: a string is never parsed back into a
  // number for rendering.
  EXPECT_EQ(render_all("%(sval)"), "1234.5, up, 2.5GB");
}

TEST(FilterRenderingDefault, DurationKeywordRendersHumanTime) { EXPECT_EQ(render_all("%(dur)"), "3d 04:00, 45s, 02:00"); }

TEST(FilterRenderingDefault, DateKeywordRendersFormattedDate) {
  EXPECT_EQ(render_all("%(stamp)"), "2025-08-24 08:00:00, 2000-01-01 00:00:00, 2009-02-13 23:31:30");
}

TEST(FilterRenderingDefault, CustomStateKeywordRendersItsName) { EXPECT_EQ(render_all("%(state)"), "started, stopped, started"); }

TEST(FilterRenderingDefault, FormatBytesRendersTheOptionalSentinelAsIs) {
  // The no-value contract carries through the format functions: the valueless
  // row renders its sentinel, not a formatted zero.
  EXPECT_EQ(render_all("%(format_bytes(oval))"), "512B, unknown, 96KB");
}

TEST(FilterRenderingDefault, DurationThresholdGoesThroughTheConverter) {
  // The custom-typed expression side of the duration keyword: `1h` means 3600
  // seconds, so a (273600) and c (7200) match and b (45) does not.
  const run_result r = run_query({"filter=dur > 1h", "warning=none", "critical=none"}, "%(name)");
  ASSERT_TRUE(r.built) << r.message;
  EXPECT_EQ(r.code, PB::Common::ResultCode::OK) << r.message;
  EXPECT_EQ(r.message, "a, c");
}

// ============================================================================
// Formatted rendering: the same templates with the options in force.
// ============================================================================

TEST(FilterRenderingFormatted, DecimalsRendersExactlyThatMany) {
  // A fixed width is the point: 700MB keeps its ".0" instead of stripping it.
  EXPECT_EQ(render_all("%(bytes)", {"decimals=1"}), "1.5KB, 2.5GB, 700.0MB");
}

TEST(FilterRenderingFormatted, ByteUnitPinsEveryValueToOneUnit) {
  EXPECT_EQ(render_all("%(bytes)", {"byte-unit=MB"}), "0.001MB, 2560MB, 700MB");
}

TEST(FilterRenderingFormatted, ByteUnitAndDecimalsMakeValuesComparable) {
  EXPECT_EQ(render_all("%(bytes)", {"byte-unit=GB", "decimals=2"}), "0.00GB, 2.50GB, 0.68GB");
}

TEST(FilterRenderingFormatted, DecimalSeparatorChangesTheRadixCharacter) {
  EXPECT_EQ(render_all("%(bytes)", {"byte-unit=GB", "decimals=2", "decimal-separator=,"}), "0,00GB, 2,50GB, 0,68GB");
}

TEST(FilterRenderingFormatted, ThousandsSeparatorGroupsTheIntegerPart) {
  EXPECT_EQ(render_all("%(bytes)", {"byte-unit=KB", "thousands-separator=,"}), "1.5KB, 2,621,440KB, 716,800KB");
}

TEST(FilterRenderingFormatted, EuropeanRendering) {
  EXPECT_EQ(render_all("%(bytes)", {"byte-unit=MB", "decimals=2", "decimal-separator=,", "thousands-separator=."}), "0,00MB, 2.560,00MB, 700,00MB");
}

TEST(FilterRenderingFormatted, FloatTemplateKeywordsFollowDecimals) {
  EXPECT_EQ(render_all("%(fraction)", {"decimals=2"}), "1234.75, 0.25, 2.71");
}

TEST(FilterRenderingFormatted, AnyOptionSwitchesFloatsToTheHumanRendering) {
  // Setting any option (here only byte-unit) routes the floats of the human
  // readable templates through the number format, whose default is "up to
  // three decimals, trailing zeros stripped" - so 2.7109375 renders "2.711"
  // where the unconfigured check rendered "2.71094".
  EXPECT_EQ(render_all("%(fraction)", {"byte-unit=KB"}), "1234.75, 0.25, 2.711");
}

TEST(FilterRenderingFormatted, IntKeywordsAreNeverReformatted) {
  // Integer keywords render digit for digit whatever the options say: no
  // grouping, no decimals. Only the byte/float/percentage renderings follow
  // the format.
  EXPECT_EQ(render_all("%(count)", {"decimals=2", "thousands-separator=,"}), "1234567, 42, 0");
}

TEST(FilterRenderingFormatted, PctHumanKeywordFollowsTheFormat) {
  EXPECT_EQ(render_all("%(pct)", {"decimals=3", "decimal-separator=,"}), "12,500, 7,250, 99,750");
}

TEST(FilterRenderingFormatted, FormatFunctionsFollowTheOptions) {
  EXPECT_EQ(render_all("%(format_number(fraction))", {"decimals=2", "decimal-separator=,"}), "1234,75, 0,25, 2,71");
}

TEST(FilterRenderingFormatted, ExplicitFunctionArgumentWinsOverTheOption) {
  EXPECT_EQ(render_all("%(format_number(fraction, 2))", {"decimals=0"}), "1234.75, 0.25, 2.71");
  EXPECT_EQ(render_all("%(format_bytes(bytes, 'GB', 2))", {"byte-unit=MB", "decimals=0"}), "0.00, 2.50, 0.68");
}

TEST(FilterRenderingFormatted, ThresholdParsingIsUntouchedBySeparators) {
  // `1234.7` must stay a '.'-radix number even with a decimal comma configured
  // for the message: the fraction keeps its meaning and only row a (1234.75)
  // crosses it.
  const run_result r = run_query({"warning=fraction > 1234.7", "critical=none", "decimal-separator=,", "thousands-separator=."}, "%(name)",
                                 "%(status)|%(warn_list)");
  ASSERT_TRUE(r.built) << r.message;
  EXPECT_EQ(r.code, PB::Common::ResultCode::WARNING) << r.message;
  EXPECT_EQ(r.message, "WARNING|a");
}

TEST(FilterRenderingFormatted, PerformanceDataIgnoresTheFormat) {
  // byte-unit must not scale the metric and the separators must not reach it:
  // the perf entry keeps the raw byte count, the raw bound and the "B" unit.
  const run_result r = run_query({"warning=bytes > 1k", "critical=none", "byte-unit=MB", "decimals=1", "decimal-separator=,"}, "%(name)");
  ASSERT_TRUE(r.built) << r.message;
  const PB::Common::PerformanceData *p = find_perf(r, "a");
  ASSERT_NE(p, nullptr) << r.message;
  ASSERT_TRUE(p->has_float_value());
  EXPECT_EQ(p->float_value().unit(), "B");
  EXPECT_DOUBLE_EQ(p->float_value().value(), 1536.0);
  ASSERT_TRUE(p->float_value().has_warning());
  EXPECT_DOUBLE_EQ(p->float_value().warning().value(), 1024.0);
}

TEST(FilterRenderingFormatted, PerfAliasesKeepThePlainRendering) {
  // The perf-syntax renderer is not a human template: a float in a perf label
  // keeps the plain '.'-radix stream rendering, whatever the options say - a
  // decimal comma in a label would travel straight into a metric store.
  const run_result r = run_query({"warning=bytes > 1k", "critical=none", "decimal-separator=,"}, "%(name)", "%(list)", "%(fraction)");
  ASSERT_TRUE(r.built) << r.message;
  EXPECT_NE(find_perf(r, "1234.75"), nullptr) << r.message;
  EXPECT_NE(find_perf(r, "0.25"), nullptr) << r.message;
  EXPECT_NE(find_perf(r, "2.71094"), nullptr) << r.message;
}

TEST(FilterRenderingFormatted, DecimalsEscapeScientificNotation) {
  // The fix for the legacy pin above: an explicit decimals renders large
  // floats in full, sign included.
  EXPECT_EQ(render_all("%(bigf)", {"decimals=2"}), "12345678.50, -9876543.25, 1000000.00");
}

TEST(FilterRenderingFormatted, ThousandsSeparatorGroupsLargeAndNegativeFloats) {
  // Grouping starts after the sign, so a negative number groups its digits
  // and keeps its '-'.
  EXPECT_EQ(render_all("%(bigf)", {"decimals=2", "thousands-separator=,"}), "12,345,678.50, -9,876,543.25, 1,000,000.00");
}

TEST(FilterRenderingFormatted, LargeIntKeywordsStayUngrouped) {
  EXPECT_EQ(render_all("%(big)", {"thousands-separator=,"}), "1234567890123, -987654321, 0");
}

TEST(FilterRenderingFormatted, SmallFloatsFollowDecimals) {
  EXPECT_EQ(render_all("%(smallf)", {"decimals=6"}), "0.015625, -0.015625, 0.500000");
  // With another option set but decimals untouched, the human default (up to
  // three, stripped) applies to small values too.
  EXPECT_EQ(render_all("%(smallf)", {"byte-unit=KB"}), "0.016, -0.016, 0.5");
}

TEST(FilterRenderingFormatted, SignedBytesFollowByteUnitAndGrouping) {
  EXPECT_EQ(render_all("%(delta)", {"byte-unit=KB", "thousands-separator=,"}), "-2,621,440KB, -1.5KB, 1KB");
}

TEST(FilterRenderingFormatted, SentinelsStringsDurationsDatesAndStatesAreNeverReformatted) {
  // Everything that is a string by nature - the optional's sentinel, string
  // keywords, durations, dates and named states - renders identically under
  // the most invasive combination of options. Only numbers follow the format.
  const std::vector<std::string> opts = {"decimals=2", "byte-unit=MB", "decimal-separator=,", "thousands-separator=."};
  EXPECT_EQ(render_all("%(oval)", opts), "512, unknown, 98304");
  EXPECT_EQ(render_all("%(sval)", opts), "1234.5, up, 2.5GB");
  EXPECT_EQ(render_all("%(dur)", opts), "3d 04:00, 45s, 02:00");
  EXPECT_EQ(render_all("%(stamp)", opts), "2025-08-24 08:00:00, 2000-01-01 00:00:00, 2009-02-13 23:31:30");
  EXPECT_EQ(render_all("%(state)", opts), "started, stopped, started");
}

TEST(FilterRenderingFormatted, FormatNumberGroupsAndSignsLargeValues) {
  EXPECT_EQ(render_all("%(format_number(bigf, 2))", {"decimal-separator=,", "thousands-separator=."}), "12.345.678,50, -9.876.543,25, 1.000.000,00");
}

TEST(FilterRenderingFormatted, FormatBytesOnSignedValuesFollowsTheFormat) {
  EXPECT_EQ(render_all("%(format_bytes(delta, 'KB', 1))", {"decimal-separator=,", "thousands-separator=."}), "-2.621.440,0, -1,5, 1,0");
}

// ============================================================================
// Errors: a format the check cannot honour is reported, never rendered wrong.
// ============================================================================

TEST(FilterRenderingErrors, OversizedDecimalsOptionIsRejected) {
  const run_result r = run_query({"filter=none", "warning=none", "critical=none", "decimals=16"}, "%(name)");
  EXPECT_FALSE(r.built);
  EXPECT_EQ(r.code, PB::Common::ResultCode::UNKNOWN) << r.message;
  EXPECT_NE(r.message.find("Invalid decimals: 16"), std::string::npos) << r.message;
}

TEST(FilterRenderingErrors, NegativeDecimalsOptionIsRejected) {
  const run_result r = run_query({"filter=none", "warning=none", "critical=none", "decimals=-2"}, "%(name)");
  EXPECT_FALSE(r.built);
  EXPECT_EQ(r.code, PB::Common::ResultCode::UNKNOWN) << r.message;
  EXPECT_NE(r.message.find("Invalid decimals: -2"), std::string::npos) << r.message;
}

TEST(FilterRenderingErrors, UnknownByteUnitOptionIsRejected) {
  const run_result r = run_query({"filter=none", "warning=none", "critical=none", "byte-unit=ZB"}, "%(name)");
  EXPECT_FALSE(r.built);
  EXPECT_EQ(r.code, PB::Common::ResultCode::UNKNOWN) << r.message;
  EXPECT_NE(r.message.find("Invalid byte-unit: ZB"), std::string::npos) << r.message;
}

TEST(FilterRenderingErrors, UnknownUnitInFormatBytesReportsInsteadOfRenderingNonsense) {
  // Before #1428 a typo like 'ZB' silently rendered value/1024^7; now the
  // render error surfaces through the error handler as UNKNOWN with the cause.
  const run_result r = run_query({"filter=none", "warning=none", "critical=none"}, "%(format_bytes(bytes, 'ZB'))");
  EXPECT_FALSE(r.built);
  EXPECT_EQ(r.code, PB::Common::ResultCode::UNKNOWN) << r.message;
  EXPECT_NE(r.message.find("Filter processing failed"), std::string::npos) << r.message;
  EXPECT_NE(r.message.find("format_bytes failed: Unknown byte unit: ZB"), std::string::npos) << r.message;
}

TEST(FilterRenderingErrors, OversizedFunctionDecimalsAreRejected) {
  // The render-crash backstop: a decimals beyond what a double can carry is an
  // error, not a multi-megabyte string.
  const run_result r = run_query({"filter=none", "warning=none", "critical=none"}, "%(format_number(fraction, 16))");
  EXPECT_FALSE(r.built);
  EXPECT_EQ(r.code, PB::Common::ResultCode::UNKNOWN) << r.message;
  EXPECT_NE(r.message.find("format_number failed: decimals must not exceed 15"), std::string::npos) << r.message;
}
