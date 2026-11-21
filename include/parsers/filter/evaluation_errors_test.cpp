// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// An expression that fails while the records are evaluated - a converter
// refusing a unit, a keyword that has no value for a record, a regular
// expression refused by the budget or the subject limit - still hands the
// engine *a* value, and the verdict used to be reached on it: `load > 5k`
// compared against 5, a refused regex read as "no match", and the check came
// back OK or CRITICAL with the error only in the debug log. These drive the
// whole pipeline a real check uses (REST-style arguments -> cli_helper ->
// modern_filter -> response) and pin that such a check now ends UNKNOWN and
// says why, while the errors that setup and the no-row force-evaluation log by
// design still do not.

#include <gtest/gtest.h>

#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/format_functions.hpp>
#include <parsers/where/helpers.hpp>
#include <parsers/where/regex_guard.hpp>
#include <string>
#include <vector>

static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace evaluation_errors {

struct data_obj {
  std::string name;
  long long load;
  long long sent;  // 0: the `loss` keyword has no value (check_ping's shape)
  long long lost;
  std::string text;

  std::string show() const { return name; }
};
typedef std::shared_ptr<data_obj> data_ptr;
typedef parsers::where::filter_handler_impl<data_ptr> native_context;

// The shape of the % converters in check_cpu / check_ping: anything but "%"
// is reported, and the bare number goes back.
static const parsers::where::value_type type_pct = parsers::where::type_custom_int_1;
static parsers::where::node_type parse_pct(data_ptr, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  const parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
  if (value.get<2>() != "%") context->error("Invalid unit: " + value.get<2>());
  return parsers::where::factory::create_int(static_cast<long long>(value.get<1>()));
}

struct filter_obj_handler : native_context {
  filter_obj_handler() {
    registry_.add_string_var("name", [](data_ptr o) { return o->name; }, "Row name");
    registry_.add_string_var("text", [](data_ptr o) { return o->text; }, "Free text");
    registry_.add_int_var("sent", [](data_ptr o) { return o->sent; }, "Plain int keyword");
    registry_.add_int_var("load", type_pct, [](data_ptr o) { return o->load; }, "Percent keyword with a converter").add_int_perf("%");
    registry_.add_int_var_w_context(
        "loss", type_pct,
        [](data_ptr o, parsers::where::evaluation_context c) -> long long {
          if (o->sent == 0) {
            c->error("No packages were sent");
            return 0;
          }
          return o->lost * 100 / o->sent;
        },
        "Percent with no value for a record that sent nothing");
    registry_.add_converter(type_pct, &parse_pct);
    parsers::where::format_functions::register_format_functions(registry_);
  }
};
typedef modern_filter::modern_filters<data_obj, filter_obj_handler> filter_type;

struct run_result {
  PB::Common::ResultCode code;
  std::string message;
  int perf_count;
};

std::vector<data_obj> default_rows() {
  return {
      {"a", 50, 4, 0, "alpha"},
      {"b", 20, 4, 1, "beta"},
  };
}

run_result run(const std::vector<std::string> &args, const std::vector<data_obj> &rows = default_rows(), const std::string &empty_state = "ignored") {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("evaluation_errors");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> helper(request, &response, data);
  filter_type filter;
  helper.add_options("", "", "", filter.get_filter_syntax(), empty_state);
  helper.add_syntax("%(status): %(list)", "%(name)=%(load)", "%(name)", "EMPTY", "");
  run_result out{PB::Common::ResultCode::UNKNOWN, "<not built>", 0};
  if (!helper.parse_options() || !helper.build_filter(filter)) {
    out.message = response.lines_size() > 0 ? response.lines(0).message() : out.message;
    out.code = response.result();
    return out;
  }
  for (const data_obj &r : rows) filter.match(std::make_shared<data_obj>(r));
  helper.post_process(filter);
  out.code = response.result();
  out.message = response.lines(0).message();
  out.perf_count = response.lines(0).perf_size();
  return out;
}

bool starts_with(const std::string &s, const std::string &prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

// Restores the regex budget the rest of the binary runs with.
struct regex_budget_guard {
  unsigned long saved = parsers::where::get_regex_budget_ms();
  ~regex_budget_guard() {
    parsers::where::set_regex_budget_ms(saved);
    parsers::where::reset_regex_budget();
  }
};

}  // namespace evaluation_errors

using namespace evaluation_errors;

// --- the baseline: nothing fails, nothing changes ---------------------------

TEST(EvaluationErrors, ValidExpressionsKeepTheirVerdict) {
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"warning=none", "critical=load > 40%"}).code);
  EXPECT_EQ(PB::Common::ResultCode::OK, run({"warning=none", "critical=load > 60%"}).code);
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run({"warning=loss >= 25%", "critical=none"}).code);
  const run_result r = run({"warning=none", "critical=text regexp 'al.*'"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code);
  EXPECT_EQ("CRITICAL: a=50, b=20", r.message);
}

// --- a converter refusing the unit ------------------------------------------

TEST(EvaluationErrors, AnInvalidUnitIsUnknownNotADifferentThreshold) {
  // `load > 40k` used to compare against a bare 40: CRITICAL for load 50.
  const run_result crit = run({"warning=none", "critical=load > 40k"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, crit.code) << crit.message;
  EXPECT_TRUE(starts_with(crit.message, "Filter evaluation failed: Invalid unit: k")) << crit.message;

  // ...and OK for a threshold the bare number clears.
  const run_result ok = run({"warning=none", "critical=load > 60k"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, ok.code) << ok.message;
  EXPECT_TRUE(starts_with(ok.message, "Filter evaluation failed: Invalid unit: k")) << ok.message;

  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, run({"filter=load > 10k", "warning=none", "critical=none"}).code);
}

TEST(EvaluationErrors, TheMessageNamesTheExpressionAsWritten) {
  // The error and the expression the operator wrote - not the tree rendered
  // with the row's values, which is what the debug log gets.
  const run_result r = run({"warning=none", "critical=load > 40k"});
  EXPECT_EQ("Filter evaluation failed: Invalid unit: k (in: load > 40k)", r.message);
}

TEST(EvaluationErrors, TheMessageCarriesNoRowValues) {
  // The subject a regular expression is refused for is the row's own value,
  // up to the 1 MiB limit and beyond: it must not reach the check's output
  // (an NRPE payload, a keyword the operator never put in a syntax).
  const std::string secret = "s3cr3t-" + std::string(parsers::where::max_regex_subject_bytes(), 'x');
  const run_result r = run({"warning=none", "critical=text regexp 'needle'"}, {{"big", 1, 4, 0, secret}});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ(std::string::npos, r.message.find("s3cr3t")) << r.message.substr(0, 200);
  EXPECT_NE(std::string::npos, r.message.find("(in: text regexp 'needle')")) << r.message;
}

TEST(EvaluationErrors, TheMessageIsOneBoundedLine) {
  // An error can quote something from the data (a getter's exception, a
  // path); the line that reaches the check is one line and capped well inside
  // an NRPE payload, without splitting a UTF-8 sequence.
  filter_type filter;
  filter.evaluation_error_count_ = 1;
  filter.evaluation_error_ = "first line\r\nsecond\tline";
  EXPECT_EQ("Filter evaluation failed: first line  second line", filter.get_evaluation_failure());

  const std::size_t cap = filter_type::max_failure_detail;
  filter.evaluation_error_ = std::string(cap - 1, 'a') + "\xC3\xA9" + std::string(500, 'b');  // 'é' straddles the cap
  const std::string capped = filter.get_evaluation_failure();
  EXPECT_EQ("Filter evaluation failed: " + std::string(cap - 1, 'a') + "...", capped);

  filter.evaluation_error_ = std::string(cap, 'c');
  EXPECT_EQ("Filter evaluation failed: " + std::string(cap, 'c'), filter.get_evaluation_failure());
}

TEST(EvaluationErrors, ASummaryOnlyThresholdThatFailsIsUnknown) {
  // `count` is evaluated once after the rows, outside any record: `5k` is
  // not a count, and `count > 5k` used to read as `count > 0` - CRITICAL.
  const run_result r = run({"warning=none", "critical=count > 5k"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_TRUE(starts_with(r.message, "Filter evaluation failed: ")) << r.message;
  // ...while a valid summary threshold keeps its verdict.
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"warning=none", "critical=count > 1"}).code);
  EXPECT_EQ(PB::Common::ResultCode::OK, run({"warning=none", "critical=count > 5"}).code);
}

TEST(EvaluationErrors, ARenderErrorIsUnknown) {
  // A function in a syntax template that fails - a byte unit that does not
  // exist - used to render as an empty placeholder in an otherwise clean
  // result.
  const std::vector<std::string> args = {"warning=none", "critical=none", "detail-syntax=%(name) %(format_bytes(load, 'ZB'))"};
  const run_result r = run(args);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("Unknown byte unit: ZB")) << r.message;
}

TEST(EvaluationErrors, PerfDataIsKeptWithTheFailure) {
  const run_result r = run({"warning=none", "critical=load > 40k"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ(2, r.perf_count);
}

// --- a keyword with no value for a record ------------------------------------

TEST(EvaluationErrors, AKeywordWithNoValueIsUnknown) {
  std::vector<data_obj> rows = default_rows();
  rows.push_back({"c", 10, 0, 0, "gamma"});
  // Row c sent nothing: its loss used to read 0% and pass. No other row
  // reaches the threshold, so nothing stands beside the failure.
  const run_result r = run({"warning=loss > 30%", "critical=loss > 50%"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_TRUE(starts_with(r.message, "Filter evaluation failed: No packages were sent")) << r.message;

  // A keyword the expression never reads does not fail the check.
  EXPECT_EQ(PB::Common::ResultCode::OK, run({"warning=none", "critical=load > 90%"}, rows).code);
}

// --- regular expressions the guard refuses -----------------------------------

TEST(EvaluationErrors, ARegexRefusedForItsSubjectSizeIsUnknownInAThreshold) {
  std::vector<data_obj> rows = default_rows();
  rows.push_back({"big", 1, 4, 0, std::string(parsers::where::max_regex_subject_bytes() + 1, 'x')});
  // A refused match used to come out OK.
  const run_result r = run({"warning=none", "critical=text regexp 'needle'"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_TRUE(starts_with(r.message, "Filter evaluation failed: Refusing to match a regular expression")) << r.message;
}

// --- a verdict the other records reached --------------------------------------

TEST(EvaluationErrors, ACriticalOtherRecordsReachedStandsBesideTheFailure) {
  // b loses 25% and is CRITICAL on its own; c's loss has no value. Replacing
  // that CRITICAL with UNKNOWN would downgrade a real alert.
  std::vector<data_obj> rows = default_rows();
  rows.push_back({"c", 10, 0, 0, "gamma"});
  const run_result r = run({"warning=loss > 5%", "critical=loss > 10%", "top-syntax=%(status): %(crit_list)"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_EQ("CRITICAL: b=20 (Filter evaluation failed: No packages were sent (in: loss > 10%))", r.message);

  // The same for a refused regular expression beside a record that matches.
  rows = default_rows();
  rows.push_back({"big", 1, 4, 0, std::string(parsers::where::max_regex_subject_bytes() + 1, 'x')});
  const run_result regex = run({"warning=none", "critical=text not regexp 'x'", "top-syntax=%(status): %(crit_list)"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, regex.code) << regex.message;
  EXPECT_TRUE(starts_with(regex.message, "CRITICAL: a=50, b=20 (Filter evaluation failed: Refusing to match")) << regex.message;
}

TEST(EvaluationErrors, AFailedRecordGetsNoVerdictOfItsOwn) {
  // c's loss would read 0%, which `loss >= 0%` matches: the failed record must
  // neither pass the filter nor fire a threshold on that substitute.
  std::vector<data_obj> rows = default_rows();
  rows.push_back({"c", 10, 0, 0, "gamma"});
  const run_result filtered = run({"filter=loss >= 0%", "warning=none", "critical=sent >= 0", "top-syntax=%(status): %(crit_list)"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, filtered.code) << filtered.message;
  EXPECT_TRUE(starts_with(filtered.message, "CRITICAL: a=50, b=20 (Filter evaluation failed: No packages were sent")) << filtered.message;

  // Only c could fire `loss < 1%` on its substitute 0: it does not, and the
  // failure is reported instead.
  const run_result only_c = run({"warning=none", "critical=loss < 1% and sent = 0"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, only_c.code) << only_c.message;
}

TEST(EvaluationErrors, AWarningIsKeptAndAnEmptyCriticalStateToo) {
  std::vector<data_obj> rows = default_rows();
  rows.push_back({"c", 10, 0, 0, "gamma"});
  // b warns at 25%; c fails.
  const run_result warn = run({"warning=loss > 5%", "critical=loss > 50%"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::WARNING, warn.code) << warn.message;
  EXPECT_NE(std::string::npos, warn.message.find("(Filter evaluation failed: ")) << warn.message;
  // A summary-only threshold that fails gives no verdict either.
  const run_result summary = run({"warning=none", "critical=count > 5k"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, summary.code) << summary.message;
}

TEST(EvaluationErrors, ARegexRefusedInTheFilterIsNotAnEmptyOk) {
  // An empty state of ok is what the log checks use: the refused record used
  // to be dropped and the check read "nothing found".
  const std::vector<data_obj> rows = {{"big", 1, 4, 0, std::string(parsers::where::max_regex_subject_bytes() + 1, 'x')}};
  const run_result r = run({"filter=text regexp 'needle'", "warning=none", "critical=none"}, rows, "ok");
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_TRUE(starts_with(r.message, "Filter evaluation failed: Refusing to match a regular expression")) << r.message;
}

TEST(EvaluationErrors, AnExhaustedRegexBudgetIsUnknown) {
  regex_budget_guard guard;
  parsers::where::set_regex_budget_ms(1);
  // Catastrophic backtracking: whichever stops it first - the budget or
  // Boost's own complexity ceiling - the match is refused, not answered.
  std::vector<data_obj> rows;
  for (int i = 0; i < 20; ++i) rows.push_back({"r" + std::to_string(i), 1, 4, 0, std::string(28, 'a') + "!"});
  const run_result r = run({"warning=none", "critical=text regexp '(a+)+$'"}, rows);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_TRUE(starts_with(r.message, "Filter evaluation failed: ")) << r.message;
}

// --- what must not change ----------------------------------------------------

TEST(EvaluationErrors, NoRowForceEvaluationIsNotAnError) {
  // With no rows the engines are force-evaluated without an object, which
  // logs by design; a mixed expression still reaches its summary verdict.
  const std::vector<data_obj> none;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"warning=none", "critical=load > 10% or count = 0"}, none).code);
  const run_result empty = run({"warning=none", "critical=load > 10%"}, none, "ok");
  EXPECT_EQ(PB::Common::ResultCode::OK, empty.code) << empty.message;
  EXPECT_EQ("EMPTY", empty.message);
}

TEST(EvaluationErrors, TextAgainstANumberStillNeverMatches) {
  // The cross-type rule: text that is not a number never matches a numeric
  // comparison, = and != alike. That is a defined answer, not a failure, so it
  // must not turn the check UNKNOWN.
  for (const char *crit : {"critical=sent = 'beta'", "critical=sent != 'beta'", "critical=sent > 'beta'", "critical=sent < 'beta'"}) {
    const run_result r = run({"warning=none", crit});
    EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << crit << ": " << r.message;
  }
  // A quoted number still joins the numeric domain.
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"warning=none", "critical=sent = '4'"}).code);
}

TEST(EvaluationErrors, TextGivenToAConverterIsNotReadAsZero) {
  // A converter keyword turns its literal into a number. Text has none, and
  // the converter used to read it as 0: `load != 'beta'` was `load != 0`.
  for (const char *crit : {"critical=load != 'beta'", "critical=load > 'beta'"}) {
    const run_result r = run({"warning=none", crit});
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << crit << ": " << r.message;
    EXPECT_TRUE(starts_with(r.message, "Filter evaluation failed: Not a number: beta")) << r.message;
  }
  // A quoted number is still a number to the converter.
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run({"warning=none", "critical=load = '50'"}).code);
}

TEST(EvaluationErrors, SetupErrorsStillFailTheBuild) {
  const run_result r = run({"warning=none", "critical=no_such_keyword > 1"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_FALSE(starts_with(r.message, "Filter evaluation failed: ")) << r.message;
}

TEST(EvaluationErrors, EachCheckStartsClean) {
  // The same filter object, matched twice: the second run's verdict must not
  // inherit the first run's failure.
  PB::Commands::QueryRequestMessage::Request request;
  request.add_arguments("warning=none");
  request.add_arguments("critical=load > 40%");
  PB::Commands::QueryResponseMessage::Response response;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> helper(request, &response, data);
  filter_type filter;
  helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  helper.add_syntax("%(status): %(list)", "%(name)", "%(name)", "EMPTY", "");
  ASSERT_TRUE(helper.parse_options());
  ASSERT_TRUE(helper.build_filter(filter));
  filter.match(std::make_shared<data_obj>(data_obj{"c", 10, 0, 0, "x"}));
  // Read a failing keyword by hand, as a stray evaluation would.
  filter.context->error("left over");
  filter.drain_render_errors();
  EXPECT_FALSE(filter.has_evaluation_errors()) << "outside an evaluation scope";
  filter.start_match();
  EXPECT_FALSE(filter.has_evaluation_errors());
  EXPECT_TRUE(filter.get_evaluation_error().empty());
}
