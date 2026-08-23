// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// End-to-end matrix test for the filter framework: every keyword type the
// registry can express (int, float, string, optional int, int+string dual,
// size-typed int, custom int with converter, int without perf) crossed with
// every comparison shape (all six relational operators, bare int / bare float
// / quoted numeric / quoted text literals, reversed operands, like/regexp/in
// and their negations, unit literals, converter units and convert()), driven
// "all the way" through the same pipeline a real check uses: REST-style
// argument strings -> cli_helper -> modern_filter -> protobuf response.
// Every case is asserted in all three roles (filter, warning, critical) and
// the response is checked for state, rendered message and performance data.
//
// The expectations here are the contract of the where engine's cross-type
// rules (see docs/docs/setup/upgrading.md): numbers win against bare numeric
// literals, quoting keeps text ordering, a non-numeric row value never
// matches a numeric comparison, and optional values with no value are
// sure-false against every number.

#include <gtest/gtest.h>

#include <boost/optional.hpp>
#include <map>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace filter_matrix {

struct data_obj {
  std::string name;
  long long ival;
  double fval;
  std::string sval;
  boost::optional<long long> oval;
  long long zval;        // byte count, registered as type_size
  std::string dval_s;    // dual string/int column (a log column shape)
  long long dval_i;

  std::string show() const { return name; }
};

typedef std::shared_ptr<data_obj> data_ptr;
typedef parsers::where::filter_handler_impl<data_ptr> native_context;

// Converter for the custom-int keyword: "Nu" means N*10, mirroring the shape
// of the duration converters (a per-keyword unit with registered precedence
// over the generic cross-type rules).
static const parsers::where::value_type type_custom_tens = parsers::where::type_custom_int_1;
static parsers::where::node_type parse_tens(data_ptr /*object*/, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  const std::list<parsers::where::node_type> tokens = subject->get_list_value(context);
  if (tokens.size() == 2) {
    auto token = tokens.begin();
    const double count = (*token)->get_float_value(context);
    return parsers::where::factory::create_int(static_cast<long long>(count * 10));
  }
  return parsers::where::factory::create_int(subject->get_int_value(context));
}

struct filter_obj_handler : native_context {
  filter_obj_handler() {
    // clang-format off
    registry_.add_string_var("name", [](data_ptr o) { return o->name; }, "Row name");
    registry_.add_int_var("ival", [](data_ptr o) { return o->ival; }, "Plain int keyword").add_int_perf();
    registry_.add_int_var("zval", parsers::where::type_size, [](data_ptr o) { return o->zval; }, "Byte-count keyword (size units)").add_int_perf("B");
    registry_.add_float("fval", [](data_ptr o) { return o->fval; }, "Float keyword").add_float_perf();
    registry_.add_string_var("sval", [](data_ptr o) { return o->sval; }, "String keyword");
    registry_.add_optional_int_var("oval", [](data_ptr o) { return o->oval; }, "unknown", "Optional int keyword").add_int_perf();
    registry_.add_string_var("dval", [](data_ptr o) { return o->dval_s; }, [](data_ptr o) { return o->dval_i; }, "Dual string/int keyword");
    registry_.add_int_var("cust", type_custom_tens, [](data_ptr o) { return o->ival * 10; }, "Custom int keyword with converter (Nu = N*10)").no_perf();
    registry_.add_int_var("quiet", [](data_ptr o) { return o->ival; }, "Int keyword that must not emit perf").no_perf();
    registry_.add_converter(type_custom_tens, &parse_tens);
    // clang-format on
  }
};

typedef modern_filter::modern_filters<data_obj, filter_obj_handler> filter_type;

// The fixed data set. Values are chosen so numeric and lexical orderings
// disagree wherever the matrix needs to tell them apart:
//   name  ival  fval   sval     oval     zval  dval
//   a     1     1.5    alpha    100      512   "7"/7
//   b     5     5.0    100      (none)   1536  "2"/2
//   c     10    10.25  90       5        2048  "x"/0
//   d     50    0.5    beta     0        4096  "30"/30
std::vector<data_obj> rows() {
  return {
      {"a", 1, 1.5, "alpha", boost::optional<long long>(100), 512, "7", 7},
      {"b", 5, 5.0, "100", boost::none, 1536, "2", 2},
      {"c", 10, 10.25, "90", boost::optional<long long>(5), 2048, "x", 0},
      {"d", 50, 0.5, "beta", boost::optional<long long>(0), 4096, "30", 30},
  };
}

struct run_result {
  PB::Common::ResultCode code;
  std::string message;
  std::vector<PB::Common::PerformanceData> perf;
  bool built;  // false when parse/build/validate failed (message carries why)
};

// Run one query through the exact pipeline a real check uses. args are the
// REST-style k=v argument strings ("filter=...", "warning=...", ...).
run_result run_query(const std::vector<std::string> &args, const std::string &top_syntax = "%(status)|%(list)|%(warn_list)|%(crit_list)",
                     const std::string &detail_syntax = "%(name)") {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("filter_matrix");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, &response, data);
  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax(top_syntax, detail_syntax, "%(name)", "EMPTY", "");

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
  }
  filter_helper.post_process(filter);
  out.built = true;
  out.code = response.result();
  out.message = response.lines_size() > 0 ? response.lines(0).message() : "";
  for (int i = 0; i < response.lines(0).perf_size(); ++i) out.perf.push_back(response.lines(0).perf(i));
  return out;
}

// Split the pinned top syntax "%(status)|%(list)|%(warn_list)|%(crit_list)".
std::vector<std::string> parts(const std::string &message) {
  std::vector<std::string> ret;
  std::string::size_type pos = 0;
  for (;;) {
    const std::string::size_type next = message.find('|', pos);
    if (next == std::string::npos) {
      ret.push_back(message.substr(pos));
      break;
    }
    ret.push_back(message.substr(pos, next - pos));
    pos = next + 1;
  }
  return ret;
}

const PB::Common::PerformanceData *find_perf(const run_result &r, const std::string &alias) {
  for (const PB::Common::PerformanceData &p : r.perf) {
    if (p.alias() == alias) return &p;
  }
  return nullptr;
}

// One matrix case: the expression and the names of the rows it must match,
// as a comma-joined list ("" = no row matches).
struct matrix_case {
  const char *expr;
  const char *matches;
};

// Run one expression in all three roles and assert the full outcome:
// - as filter: OK, %(list) is exactly the matching rows;
// - as warning: WARNING iff any row matches, %(warn_list) is exactly them;
// - as critical: CRITICAL iff any row matches, %(crit_list) is exactly them.
void expect_all_roles(const matrix_case &c) {
  const std::string expected(c.matches);
  const bool any = !expected.empty();
  {
    SCOPED_TRACE(std::string("filter=") + c.expr);
    const run_result r = run_query({std::string("filter=") + c.expr, "warning=none", "critical=none"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_EQ(r.code, PB::Common::ResultCode::OK) << r.message;
    if (!any) {
      // A filter that matches nothing renders the empty-syntax instead of
      // the top syntax.
      EXPECT_EQ(r.message, "EMPTY");
    } else {
      ASSERT_EQ(parts(r.message).size(), 4u) << r.message;
      EXPECT_EQ(parts(r.message)[1], expected) << r.message;
    }
  }
  {
    SCOPED_TRACE(std::string("warning=") + c.expr);
    const run_result r = run_query({std::string("warning=") + c.expr, "critical=none"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_EQ(r.code, any ? PB::Common::ResultCode::WARNING : PB::Common::ResultCode::OK) << r.message;
    ASSERT_EQ(parts(r.message).size(), 4u) << r.message;
    EXPECT_EQ(parts(r.message)[2], expected) << r.message;
  }
  {
    SCOPED_TRACE(std::string("critical=") + c.expr);
    const run_result r = run_query({std::string("critical=") + c.expr, "warning=none"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_EQ(r.code, any ? PB::Common::ResultCode::CRITICAL : PB::Common::ResultCode::OK) << r.message;
    ASSERT_EQ(parts(r.message).size(), 4u) << r.message;
    EXPECT_EQ(parts(r.message)[3], expected) << r.message;
  }
}

void run_matrix(const std::vector<matrix_case> &cases) {
  for (const matrix_case &c : cases) expect_all_roles(c);
}

}  // namespace filter_matrix

using namespace filter_matrix;

// ============================================================================
// Plain int keyword: ival = {a:1, b:5, c:10, d:50}
// ============================================================================

TEST(FilterMatrixInt, AllRelationalOperatorsAgainstBareInt) {
  run_matrix({
      {"ival = 5", "b"},
      {"ival != 5", "a, c, d"},
      {"ival < 5", "a"},
      {"ival <= 5", "a, b"},
      {"ival > 5", "c, d"},
      {"ival >= 5", "b, c, d"},
  });
}

TEST(FilterMatrixInt, BareFloatLiteralWidensInsteadOfRounding) {
  // 2.5 must mean 2.5: were the literal rounded into the int domain (the
  // pre-fix behaviour) `ival > 2.5` would evaluate as `ival > 3` — same match
  // set here, so the equality cases below are the discriminators.
  run_matrix({
      {"ival > 2.5", "b, c, d"},
      {"ival < 2.5", "a"},
      {"ival = 2.5", ""},   // no int equals 2.5; rounding would compare = 3
      {"ival != 2.5", "a, b, c, d"},
      {"ival >= 4.5", "b, c, d"},
      {"ival <= 0.5", ""},
  });
}

TEST(FilterMatrixInt, QuotedNumericLiteralComparesAsNumber) {
  run_matrix({
      {"ival = '5'", "b"},
      {"ival > '5'", "c, d"},
      {"ival <= '5'", "a, b"},
      {"ival != '5'", "a, c, d"},
  });
}

TEST(FilterMatrixInt, QuotedTextLiteralNeverMatchesAnIntKeyword) {
  // A quoted literal that is not a number cannot join the numeric domain;
  // the built-in conversion then pulls the LITERAL into the int domain,
  // where a non-numeric string fails to convert (one logged error per row)
  // and every comparison is false. Note this differs from the optional
  // keyword, whose sentinel drags the same shape into a lexical comparison.
  run_matrix({
      {"ival = 'beta'", ""},
      {"ival < 'beta'", ""},
      {"ival > 'beta'", ""},
  });
}

TEST(FilterMatrixInt, ReversedOperandsBehaveSymmetrically) {
  run_matrix({
      {"5 < ival", "c, d"},
      {"5 >= ival", "a, b"},
      {"2.5 > ival", "a"},
      {"'5' = ival", "b"},
      {"50 = ival", "d"},
  });
}

TEST(FilterMatrixInt, InAndNotIn) {
  run_matrix({
      {"ival in (1, 10)", "a, c"},
      {"ival not in (1, 10)", "b, d"},
      {"ival in (2, 3)", ""},
  });
}

TEST(FilterMatrixInt, NegativeLiteralAndBooleanCombinations) {
  run_matrix({
      {"ival > -1", "a, b, c, d"},
      {"ival > 1 and ival < 50", "b, c"},
      {"ival = 1 or ival = 50", "a, d"},
      {"(ival = 1 or ival = 50) and ival > 5", "d"},
      {"not (ival > 5)", "a, b"},
  });
}

// ============================================================================
// Float keyword: fval = {a:1.5, b:5.0, c:10.25, d:0.5}
// ============================================================================

TEST(FilterMatrixFloat, AllRelationalOperatorsAgainstBareNumbers) {
  run_matrix({
      {"fval = 5", "b"},
      {"fval != 5", "a, c, d"},
      {"fval > 5", "c"},
      {"fval >= 5", "b, c"},
      {"fval < 1.5", "d"},
      {"fval <= 1.5", "a, d"},
      {"fval > 2.5", "b, c"},
      {"fval = 10.25", "c"},
  });
}

TEST(FilterMatrixFloat, QuotedNumericLiteralComparesAsNumber) {
  run_matrix({
      {"fval = '5'", "b"},
      {"fval > '2.5'", "b, c"},
      {"fval <= '1.5'", "a, d"},
      // Non-numeric quoted text cannot convert into the float domain:
      // silently false for every row (same contract as the int keyword).
      {"fval = 'x'", ""},
  });
}

TEST(FilterMatrixFloat, ReversedOperands) {
  run_matrix({
      {"5 <= fval", "b, c"},
      {"1.5 >= fval", "a, d"},
  });
}

// ============================================================================
// String keyword: sval = {a:"alpha", b:"100", c:"90", d:"beta"}
// ============================================================================

TEST(FilterMatrixString, QuotedLiteralsCompareAsText) {
  run_matrix({
      {"sval = 'alpha'", "a"},
      {"sval != 'alpha'", "b, c, d"},
      // Lexical ordering: "100" < "95" ('1' < '9'), "alpha"/"beta" > "95".
      {"sval > '95'", "a, d"},
      {"sval < '95'", "b, c"},
      {"sval >= 'alpha'", "a, d"},
      {"sval <= '100'", "b"},
  });
}

TEST(FilterMatrixString, BareNumberComparesNumericallyAndTextNeverMatches) {
  // Numbers win: the row value is parsed per record; "alpha"/"beta" are a
  // certain non-match for EVERY operator (the no_value contract), including
  // = and != — they are not "different from 95", they are not comparable.
  run_matrix({
      {"sval > 95", "b"},
      {"sval < 95", "c"},
      {"sval = 100", "b"},
      {"sval != 95", "b, c"},
      {"sval >= 90", "b, c"},
      {"sval <= 90", "c"},
      {"sval > 2.5", "b, c"},
  });
}

TEST(FilterMatrixString, ReversedBareNumberAlsoComparesNumerically) {
  run_matrix({
      {"95 < sval", "b"},
      {"95 > sval", "c"},
  });
}

TEST(FilterMatrixString, LikeRegexpAndTheirNegations) {
  run_matrix({
      {"sval like 'alph'", "a"},
      {"sval not like 'alph'", "b, c, d"},
      {"sval like '0'", "b, c"},
      {"sval regexp '^[0-9]+$'", "b, c"},
      {"sval not regexp '^[0-9]+$'", "a, d"},
      {"sval regexp '^(alpha|beta)$'", "a, d"},
  });
}

TEST(FilterMatrixString, InAndNotIn) {
  run_matrix({
      {"sval in ('alpha', 'beta')", "a, d"},
      {"sval not in ('alpha', 'beta')", "b, c"},
      {"sval in ('missing')", ""},
  });
}

// ============================================================================
// Optional int keyword: oval = {a:100, b:(no value), c:5, d:0}
// ============================================================================

TEST(FilterMatrixOptional, NumericComparisonsSkipTheValuelessRow) {
  // b has no value: sure-false against every number, for every operator —
  // deliberately including != (nothing numeric can be said about it).
  run_matrix({
      {"oval = 5", "c"},
      {"oval != 5", "a, d"},
      {"oval > 5", "a"},
      {"oval >= 5", "a, c"},
      {"oval < 5", "d"},
      {"oval <= 5", "c, d"},
      {"oval > 2.5", "a, c"},
      {"oval = 0", "d"},
  });
}

TEST(FilterMatrixOptional, UnknownSentinelIsTheDocumentedProbe) {
  run_matrix({
      {"oval = 'unknown'", "b"},
      // The sentinel probe drags the comparison into the string domain, so
      // != matches every row whose rendered value differs from "unknown".
      {"oval != 'unknown'", "a, c, d"},
  });
}

TEST(FilterMatrixOptional, QuotedNumberStaysInTheSentinelStringDomain) {
  // Pinned on purpose: a QUOTED number against an optional keyword is a
  // lexical comparison against the rendered value — including the "unknown"
  // sentinel itself ("unknown" > "50" because 'u' > '5'). Bare numbers are
  // the supported numeric form; this documents why quoting them is wrong.
  run_matrix({
      {"oval > '50'", "b"},
      {"oval < '50'", "a, c, d"},
  });
}

// ============================================================================
// Dual string/int keyword (log column shape): dval = {a:"7", b:"2", c:"x", d:"30"}
// with int accessor {a:7, b:2, c:0, d:30}
// ============================================================================

TEST(FilterMatrixDual, QuotedLiteralUsesTheStringAccessor) {
  run_matrix({
      {"dval = '7'", "a"},
      {"dval = 'x'", "c"},
      // Lexical: "7" > "5", "x" > "5"; "2" and "30" sort below "5".
      {"dval > '5'", "a, c"},
  });
}

TEST(FilterMatrixDual, BareNumberUsesTheIntAccessor) {
  run_matrix({
      {"dval > 5", "a, d"},
      {"dval <= 5", "b, c"},
      {"dval = 30", "d"},
      // The decimal threshold is served losslessly from the int accessor.
      {"dval > 2.5", "a, d"},
  });
}

// ============================================================================
// Size-typed int keyword: zval = {a:512, b:1536, c:2048, d:4096}
// ============================================================================

TEST(FilterMatrixSize, UnitLiteralsIncludingFractions) {
  run_matrix({
      {"zval > 1k", "b, c, d"},
      // 1.5k = 1536 exactly: b (=1536) is NOT greater. The truncating
      // pre-fix behaviour (1.5k -> 1k) would have matched b.
      {"zval > 1.5k", "c, d"},
      {"zval >= 1.5k", "b, c, d"},
      {"zval < 2k", "a, b"},
      {"zval = 2k", "c"},
  });
}

// ============================================================================
// Custom int keyword with converter: cust = {a:10, b:50, c:100, d:500},
// converter "Nu" = N*10 (registered converters take precedence)
// ============================================================================

TEST(FilterMatrixCustom, ConverterUnitsTakePrecedence) {
  run_matrix({
      {"cust > 7u", "c, d"},    // 70
      {"cust = 5u", "b"},       // 50
      {"cust > 2.5u", "b, c, d"},  // 25: the fraction survives the converter
      {"cust > 75", "c, d"},    // plain numbers stay plain
  });
}

// ============================================================================
// Known engine gaps, pinned so a fix flips these tests deliberately.
// Both reproduce identically on the pre-cross-type-fix engine — they are
// long-standing limitations, not regressions.
// ============================================================================

TEST(FilterMatrixKnownGaps, BareNumberAgainstSizeTypedKeywordFailsValidation) {
  // A bare byte count against a type_size keyword cannot be compared:
  // int_value::infer_type refuses the type_size suggestion (type_is_int(size)
  // is true, so it returns type_int unchanged) and can_convert has no
  // int<->size rule, so validation fails loudly. Unit literals (2k) and, for
  // whole kilobytes, `zval > 2048` written as `zval > 2k` are the supported
  // forms. NB: the shipped check_registry_value sample `warn=size > 4096`
  // hits exactly this.
  const run_result r = run_query({"filter=zval > 2000", "warning=none", "critical=none"});
  EXPECT_FALSE(r.built);
  EXPECT_EQ(r.code, PB::Common::ResultCode::UNKNOWN) << r.message;
}

TEST(FilterMatrixKnownGaps, ExplicitConvertInAFilterStringSilentlyMatchesNothing) {
  // unary_fun::infer_type never adopts a type (always type_tbd), so in
  // `convert(zval, 'k') > 1` the literal's int type wins the inference and
  // function_convert refuses a unit conversion to plain int — every row
  // errors and the comparison is silently false. No shipped filter uses the
  // explicit convert() form (unit literals cover the need); pinned so a
  // future fix is a deliberate change.
  const run_result r = run_query({"filter=convert(zval, 'k') > 1", "warning=none", "critical=none"});
  ASSERT_TRUE(r.built) << r.message;
  EXPECT_EQ(r.code, PB::Common::ResultCode::OK) << r.message;
  EXPECT_EQ(r.message, "EMPTY");
}

// ============================================================================
// Summary keywords in warn/crit (count, total)
// ============================================================================

TEST(FilterMatrixSummary, CountAndTotalDriveTheState) {
  {
    // filter matches b,c,d (count=3) -> warn on count > 2 fires.
    const run_result r = run_query({"filter=ival > 1", "warning=count > 2", "critical=none"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_EQ(r.code, PB::Common::ResultCode::WARNING) << r.message;
  }
  {
    const run_result r = run_query({"filter=ival > 1", "warning=count > 3", "critical=none"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_EQ(r.code, PB::Common::ResultCode::OK) << r.message;
  }
  {
    // Fractional threshold on the fixed-int summary counter: 3 > 2.5.
    const run_result r = run_query({"filter=ival > 1", "warning=count > 2.5", "critical=none"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_EQ(r.code, PB::Common::ResultCode::WARNING) << r.message;
  }
  {
    const run_result r = run_query({"warning=none", "critical=total = 4"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_EQ(r.code, PB::Common::ResultCode::CRITICAL) << r.message;
  }
}

// ============================================================================
// Performance data: values, warn/crit bounds, suppression
// ============================================================================

TEST(FilterMatrixPerf, IntKeywordEmitsValueAndBothBounds) {
  const run_result r = run_query({"warning=ival > 5", "critical=ival > 20"});
  ASSERT_TRUE(r.built) << r.message;
  // One numeric series per row, labelled by the perf syntax (%(name)).
  for (const data_obj &row : rows()) {
    const PB::Common::PerformanceData *p = find_perf(r, row.name);
    ASSERT_NE(p, nullptr) << "missing perf for row " << row.name << " in " << r.message;
    ASSERT_TRUE(p->has_float_value()) << row.name;
    EXPECT_DOUBLE_EQ(p->float_value().value(), static_cast<double>(row.ival)) << row.name;
    ASSERT_TRUE(p->float_value().has_warning()) << row.name;
    EXPECT_DOUBLE_EQ(p->float_value().warning().value(), 5.0) << row.name;
    ASSERT_TRUE(p->float_value().has_critical()) << row.name;
    EXPECT_DOUBLE_EQ(p->float_value().critical().value(), 20.0) << row.name;
  }
}

TEST(FilterMatrixPerf, FloatKeywordKeepsFractionalBounds) {
  const run_result r = run_query({"warning=fval > 2.5", "critical=none"});
  ASSERT_TRUE(r.built) << r.message;
  const PB::Common::PerformanceData *p = find_perf(r, "a");
  ASSERT_NE(p, nullptr) << r.message;
  ASSERT_TRUE(p->has_float_value());
  EXPECT_DOUBLE_EQ(p->float_value().value(), 1.5);
  ASSERT_TRUE(p->float_value().has_warning());
  EXPECT_DOUBLE_EQ(p->float_value().warning().value(), 2.5);
}

TEST(FilterMatrixPerf, OptionalKeywordSuppressesTheValuelessRow) {
  const run_result r = run_query({"warning=oval > 5", "critical=none"});
  ASSERT_TRUE(r.built) << r.message;
  EXPECT_NE(find_perf(r, "a"), nullptr) << r.message;
  EXPECT_NE(find_perf(r, "c"), nullptr) << r.message;
  EXPECT_NE(find_perf(r, "d"), nullptr) << r.message;
  // b has no value: no entry at all rather than a made-up sentinel.
  EXPECT_EQ(find_perf(r, "b"), nullptr) << r.message;
}

TEST(FilterMatrixPerf, NoPerfKeywordAndStringKeywordEmitNothing) {
  {
    const run_result r = run_query({"warning=quiet > 5", "critical=none"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_TRUE(r.perf.empty()) << r.message;
  }
  {
    const run_result r = run_query({"warning=sval = 'alpha'", "critical=none"});
    ASSERT_TRUE(r.built) << r.message;
    EXPECT_TRUE(r.perf.empty()) << r.message;
  }
}

TEST(FilterMatrixPerf, SizeKeywordKeepsFractionalUnitBound) {
  const run_result r = run_query({"warning=zval > 1.5k", "critical=none"});
  ASSERT_TRUE(r.built) << r.message;
  const PB::Common::PerformanceData *p = find_perf(r, "a");
  ASSERT_NE(p, nullptr) << r.message;
  ASSERT_TRUE(p->has_float_value());
  EXPECT_DOUBLE_EQ(p->float_value().value(), 512.0);
  ASSERT_TRUE(p->float_value().has_warning());
  // 1.5k = 1536 — not the truncated 1024.
  EXPECT_DOUBLE_EQ(p->float_value().warning().value(), 1536.0);
}

TEST(FilterMatrixPerf, FractionalSummaryThresholdKeepsCountPerf) {
  // Regression companion to unary_fun::find_performance_data: the convert
  // wrapper around the fixed-int summary counter must not cost the count
  // series its perf entry.
  const run_result with_fraction = run_query({"filter=ival > 1", "warning=count > 2.5", "critical=none"});
  ASSERT_TRUE(with_fraction.built) << with_fraction.message;
  const PB::Common::PerformanceData *p = find_perf(with_fraction, "count");
  ASSERT_NE(p, nullptr) << with_fraction.message;
  ASSERT_TRUE(p->has_float_value());
  EXPECT_DOUBLE_EQ(p->float_value().value(), 3.0);

  const run_result with_int = run_query({"filter=ival > 1", "warning=count > 2", "critical=none"});
  ASSERT_TRUE(with_int.built) << with_int.message;
  ASSERT_NE(find_perf(with_int, "count"), nullptr) << with_int.message;
}

// ============================================================================
// Syntax rendering
// ============================================================================

TEST(FilterMatrixSyntax, DetailSyntaxRendersEveryKeywordType) {
  const run_result r =
      run_query({"filter=name = 'b'", "warning=none", "critical=none"}, "%(list)", "%(name)/%(ival)/%(fval)/%(sval)/%(oval)/%(zval)/%(dval)");
  ASSERT_TRUE(r.built) << r.message;
  // Row b renders its valueless optional as the registered no-value string.
  EXPECT_EQ(r.message, "b/5/5/100/unknown/1536/2");
}

TEST(FilterMatrixSyntax, TopSyntaxCountersAndStatus) {
  const run_result r = run_query({"warning=ival > 5", "critical=ival > 20", "filter=none"},
                                 "%(status)/%(count)/%(total)/%(ok_count)/%(warn_count)/%(crit_count)/%(problem_count)");
  ASSERT_TRUE(r.built) << r.message;
  // 4 rows: a,b ok; c warns (10 > 5); d crits (50 > 20).
  EXPECT_EQ(r.code, PB::Common::ResultCode::CRITICAL);
  EXPECT_EQ(r.message, "CRITICAL/4/4/2/1/1/2");
}

TEST(FilterMatrixSyntax, ProblemListNamesTheOffendingRows) {
  const run_result r = run_query({"warning=ival > 5", "critical=ival > 20", "filter=none"}, "%(problem_list)");
  ASSERT_TRUE(r.built) << r.message;
  EXPECT_EQ(r.message, "c, d");
}

TEST(FilterMatrixSyntax, EmptyFilterUsesEmptySyntaxAndEmptyState) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("filter_matrix");
  request.add_arguments("filter=ival > 100");
  request.add_arguments("warning=none");
  request.add_arguments("critical=none");
  request.add_arguments("empty-state=critical");
  PB::Commands::QueryResponseMessage::Response response;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, &response, data);
  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("%(list)", "%(name)", "%(name)", "EMPTY", "");
  ASSERT_TRUE(filter_helper.parse_options());
  ASSERT_TRUE(filter_helper.build_filter(filter));
  for (const data_obj &row : rows()) filter.match(std::make_shared<data_obj>(row));
  filter_helper.post_process(filter);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL);
  EXPECT_EQ(response.lines(0).message(), "EMPTY");
}

// ============================================================================
// Errors surface as UNKNOWN, not as a silent OK
// ============================================================================

TEST(FilterMatrixErrors, UnknownKeywordFailsTheQuery) {
  const run_result r = run_query({"filter=bogus > 5", "warning=none", "critical=none"});
  EXPECT_FALSE(r.built);
  EXPECT_EQ(r.code, PB::Common::ResultCode::UNKNOWN) << r.message;
}

TEST(FilterMatrixErrors, MalformedExpressionFailsTheQuery) {
  const run_result r = run_query({"filter=ival >", "warning=none", "critical=none"});
  EXPECT_FALSE(r.built);
  EXPECT_EQ(r.code, PB::Common::ResultCode::UNKNOWN) << r.message;
}
