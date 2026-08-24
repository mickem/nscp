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
// The fixture mirrors what a real check registers: a byte-valued keyword with
// a human string getter (check_drivesize's `used`), a percentage keyword
// (`used_pct`), plain float and int keywords, and the shared format functions
// (format_bytes/format_number/convert_bytes). All values are exact binary
// fractions chosen so no assertion sits on a round-half tie.
//
// The three fixed contracts hold throughout: the options touch the *message*
// only (performance data keeps raw values and the '.' radix), threshold
// parsing is untouched, and an unconfigured check renders what it always did.

#include <gtest/gtest.h>

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

  std::string show() const { return name; }
};

typedef std::shared_ptr<data_obj> data_ptr;
typedef parsers::where::filter_handler_impl<data_ptr> native_context;

struct filter_obj_handler : native_context {
  filter_obj_handler() {
    // clang-format off
    registry_.add_string_var("name", [](data_ptr o) { return o->name; }, "Row name");
    registry_.add_int_var("bytes", parsers::where::type_size, [](data_ptr o) { return o->bytes; }, "Byte-count keyword (size units)").add_int_perf("B");
    registry_.add_float("fraction", [](data_ptr o) { return o->fraction; }, "Float keyword").add_float_perf();
    registry_.add_int_var("count", [](data_ptr o) { return o->count; }, "Plain int keyword").no_perf();
    registry_.add_float("pct", [](data_ptr o) { return o->pct; }, "Percentage keyword").no_perf();
    // clang-format on

    // The human renderings a template resolves for %(bytes) / %(pct) - the
    // same shape check_drivesize registers for %(used) / %(used_pct).
    registry_.add_human_string_context(
        "bytes", [](data_ptr o, parsers::where::evaluation_context context) { return str::format::format_byte_units(o->bytes, context->get_number_format()); },
        "");
    registry_.add_human_string_context(
        "pct", [](data_ptr o, parsers::where::evaluation_context context) { return str::format::format_pct(o->pct, context->get_number_format()); }, "");

    parsers::where::format_functions::register_format_functions(registry_);
  }
};

typedef modern_filter::modern_filters<data_obj, filter_obj_handler> filter_type;

// The fixed data set. Every fraction is an exact binary number and no
// assertion below lands on a round-half tie, so the expected strings are the
// same on every platform:
//   name  bytes                      fraction    count    pct
//   a     1536       (1.5KB)         1234.75     1234567  12.5
//   b     2684354560 (2.5GB)         0.25        42       7.25
//   c     734003200  (700MB)         2.7109375   0        99.75
std::vector<data_obj> rows() {
  return {
      {"a", 1536, 1234.75, 1234567, 12.5},
      {"b", 2684354560LL, 0.25, 42, 7.25},
      {"c", 734003200LL, 2.7109375, 0, 99.75},
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
