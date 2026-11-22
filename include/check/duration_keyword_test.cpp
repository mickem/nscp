// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Units in filter expressions, driven through the pipeline a real check uses
// (REST-style arguments -> cli_helper -> modern_filter -> response).
//
// A keyword that counts milliseconds (check_ping's `time`) took no unit at
// all: `time > 1s` reached the engine as convert(1, 's'), which only a date or
// a size can take, and the bound was read as 0 - every ping warned. Days
// keywords (`expires_in`) were the same. And a unit was one letter: `500ms`
// and `10GB` did not parse, while the converters that did see one read it by
// its first letter.

#include <gtest/gtest.h>

#include <boost/optional.hpp>
#include <check/duration_keyword.hpp>
#include <ctime>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace duration_keyword_test {

struct data_obj {
  std::string name;
  long long time;   // milliseconds
  long long age;    // seconds; -1 = never
  long long days;   // whole days; -1 = unknown
  long long bytes;  // a size
  boost::optional<long long> last;  // seconds, or no value yet

  std::string show() const { return name; }
};
typedef std::shared_ptr<data_obj> data_ptr;
typedef parsers::where::filter_handler_impl<data_ptr> native_context;

static const parsers::where::value_type type_age = parsers::where::type_custom_int_1;

struct filter_obj_handler : native_context {
  filter_obj_handler() {
    registry_.add_string_var("name", [](data_ptr o) { return o->name; }, "Row name");
    registry_.add_int_var("time", duration_keyword::type_milliseconds, [](data_ptr o) { return o->time; }, "Milliseconds").add_int_perf("ms");
    registry_.add_int_var("age", type_age, [](data_ptr o) { return o->age; }, "Seconds").add_int_perf("s");
    registry_.add_int_var("days", duration_keyword::type_days, [](data_ptr o) { return o->days; }, "Days").add_int_perf("d");
    registry_.add_int_var("bytes", parsers::where::type_size, [](data_ptr o) { return o->bytes; }, "Bytes").add_int_perf("B");
    registry_.add_int_var("written", parsers::where::type_date, [](data_ptr o) { return o->age; }, "Epoch seconds").no_perf();
    registry_.add_optional_int_var("plain", [](data_ptr o) { return o->last; }, "unknown", "Plain optional").no_perf();
    registry_.add_optional_int_var("last", type_age, [](data_ptr o) { return o->last; }, "unknown", "Seconds, or unknown").no_perf();
    registry_.add_converter(duration_keyword::type_milliseconds, &duration_keyword::parse_duration_ms<data_ptr>);
    registry_.add_converter(type_age, &duration_keyword::parse_duration<data_ptr>);
    registry_.add_converter(duration_keyword::type_days, &duration_keyword::parse_duration_days<data_ptr>);
  }
};
typedef modern_filter::modern_filters<data_obj, filter_obj_handler> filter_type;

struct run_result {
  PB::Common::ResultCode code;
  std::string message;
  // An error raised while the record was evaluated. Until evaluation errors
  // end the check UNKNOWN on their own, this is where a refused unit shows.
  bool evaluation_failed;
  std::string errors;
};

run_result run(const std::string &critical, const data_obj &row) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("duration_keyword");
  request.add_arguments("warning=none");
  request.add_arguments("critical=" + critical);
  PB::Commands::QueryResponseMessage::Response response;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> helper(request, &response, data);
  filter_type filter;
  helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  helper.add_syntax("%(status): %(list)", "%(name)", "%(name)", "EMPTY", "");
  run_result out{PB::Common::ResultCode::UNKNOWN, "<not built>", false, ""};
  if (!helper.parse_options() || !helper.build_filter(filter)) {
    out.message = response.lines_size() > 0 ? response.lines(0).message() : out.message;
    out.code = response.result();
    return out;
  }
  filter.match(std::make_shared<data_obj>(row));
  // warning and critical are evaluated in match_post(), inside post_process.
  helper.post_process(filter);
  out.evaluation_failed = filter.has_errors();
  out.errors = filter.get_errors();
  out.code = response.result();
  out.message = response.lines(0).message();
  return out;
}

const PB::Common::ResultCode OK = PB::Common::ResultCode::OK;
const PB::Common::ResultCode CRITICAL = PB::Common::ResultCode::CRITICAL;

data_obj ping(const long long ms) { return data_obj{"host", ms, 0, 0, 0, boost::none}; }
data_obj aged(const long long seconds) { return data_obj{"job", 0, seconds, 0, 0, boost::none}; }
data_obj days(const long long d) { return data_obj{"cert", 0, 0, d, 0, boost::none}; }
data_obj sized(const long long b) { return data_obj{"file", 0, 0, 0, b, boost::none}; }
data_obj last(const boost::optional<long long> s) { return data_obj{"sync", 0, 0, 0, 0, s}; }

// The verdict of a run that must have evaluated cleanly.
PB::Common::ResultCode verdict(const std::string &critical, const data_obj &row) {
  const run_result r = run(critical, row);
  EXPECT_FALSE(r.evaluation_failed) << critical << ": " << r.message << " / " << r.errors;
  return r.code;
}

}  // namespace duration_keyword_test

using namespace duration_keyword_test;

TEST(DurationKeyword, AMillisecondKeywordKeepsABareNumber) {
  EXPECT_EQ(CRITICAL, verdict("time > 200", ping(300)));
  EXPECT_EQ(OK, verdict("time > 200", ping(100)));
}

TEST(DurationKeyword, AMillisecondKeywordTakesADurationUnit) {
  EXPECT_EQ(OK, verdict("time > 1s", ping(3)));
  EXPECT_EQ(CRITICAL, verdict("time > 1s", ping(1500)));
  EXPECT_EQ(OK, verdict("time > 1.5s", ping(1400)));
  EXPECT_EQ(CRITICAL, verdict("time > 1.5s", ping(1600)));
  EXPECT_EQ(CRITICAL, verdict("time > 500ms", ping(600)));
  EXPECT_EQ(OK, verdict("time > 500ms", ping(400)));
  EXPECT_EQ(OK, verdict("time > 2m", ping(110000)));
  EXPECT_EQ(CRITICAL, verdict("time > 2min", ping(130000)));
}

TEST(DurationKeyword, ASecondsKeywordTakesADurationUnit) {
  // 5000ms is 5 seconds, not 5000 minutes.
  EXPECT_EQ(CRITICAL, verdict("age > 5000ms", aged(10)));
  EXPECT_EQ(OK, verdict("age > 5000ms", aged(3)));
  EXPECT_EQ(CRITICAL, verdict("age > 30m", aged(1801)));
  EXPECT_EQ(OK, verdict("age > 30min", aged(1799)));
  EXPECT_EQ(CRITICAL, verdict("age > 2.5h", aged(9001)));
  EXPECT_EQ(OK, verdict("age > 2.5h", aged(8999)));
  EXPECT_EQ(CRITICAL, verdict("age > 1w", aged(604801)));
}

TEST(DurationKeyword, ADaysKeywordTakesDaysAndWeeks) {
  EXPECT_EQ(CRITICAL, verdict("days < 30", days(20)));
  EXPECT_EQ(CRITICAL, verdict("days < 2w", days(13)));
  EXPECT_EQ(OK, verdict("days < 2w", days(15)));
  EXPECT_EQ(CRITICAL, verdict("days < 30d", days(29)));
  EXPECT_EQ(CRITICAL, verdict("days = -1", days(-1)));
}

TEST(DurationKeyword, ADaysKeywordRefusesAUnitBelowADay) {
  EXPECT_TRUE(run("days < 12h", days(0)).evaluation_failed);
  EXPECT_TRUE(run("days < 30m", days(0)).evaluation_failed);
}

TEST(DurationKeyword, APlainIntegerSentinelPassesThrough) {
  EXPECT_EQ(CRITICAL, verdict("age = -1", aged(-1)));
  EXPECT_EQ(OK, verdict("age = -1", aged(0)));
}

TEST(DurationKeyword, AUnitThatIsNotADurationIsReported) {
  const run_result ms = run("time > 1x", ping(3));
  EXPECT_TRUE(ms.evaluation_failed) << ms.message;
  const run_result s = run("age > 10q", aged(3));
  EXPECT_TRUE(s.evaluation_failed) << s.message;
  const run_result mo = run("age > 1mo", aged(3));
  EXPECT_TRUE(mo.evaluation_failed) << mo.message;
}

// The 'unknown' probe of an optional keyword is a string comparison when the
// keyword has no value. With one, the literal is asked for a number, which
// logs an error until evaluation errors are reworked (a plain optional
// keyword does the same) - but the verdict is right either way, and the
// converter changes nothing about it.
TEST(DurationKeyword, TheUnknownProbeOfAnOptionalKeywordStillWorks) {
  EXPECT_EQ(CRITICAL, verdict("last = 'unknown'", last(boost::none)));
  EXPECT_EQ(OK, verdict("last != 'unknown'", last(boost::none)));
  EXPECT_EQ(OK, verdict("last > 1h", last(boost::none)));
  EXPECT_EQ(CRITICAL, verdict("last > 1h", last(7200)));
  EXPECT_EQ(OK, verdict("last > 1h", last(60)));
  for (const std::string &op : {"=", "!="}) {
    const run_result with_converter = run("last " + op + " 'unknown'", last(60));
    const run_result plain = run("plain " + op + " 'unknown'", last(60));
    EXPECT_EQ(plain.code, with_converter.code) << op;
    EXPECT_EQ(plain.evaluation_failed, with_converter.evaluation_failed) << op << ": " << with_converter.errors;
  }
  EXPECT_EQ(OK, run("last = 'unknown'", last(60)).code);
  EXPECT_EQ(CRITICAL, run("last != 'unknown'", last(60)).code);
}

// The engine's own conversions: sizes and dates.
TEST(ExpressionUnits, ASizeTakesAMultiLetterUnit) {
  const long long gib = 1024LL * 1024 * 1024;
  EXPECT_EQ(CRITICAL, verdict("bytes > 1g", sized(2 * gib)));
  EXPECT_EQ(CRITICAL, verdict("bytes > 1GB", sized(2 * gib)));
  EXPECT_EQ(OK, verdict("bytes > 1GB", sized(gib / 2)));
  EXPECT_EQ(CRITICAL, verdict("bytes > 1GiB", sized(2 * gib)));
  EXPECT_EQ(CRITICAL, verdict("bytes > 500kb", sized(600 * 1024)));
  EXPECT_EQ(CRITICAL, verdict("bytes > 1.5M", sized(2 * 1024 * 1024)));
}

TEST(ExpressionUnits, ASizeRefusesAUnitThatIsNotOne) {
  EXPECT_TRUE(run("bytes > 5x", sized(10)).evaluation_failed);
  EXPECT_TRUE(run("bytes > 5gbb", sized(10)).evaluation_failed);
  EXPECT_TRUE(run("bytes > 5%", sized(10)).evaluation_failed);
}

TEST(ExpressionUnits, ADateReadsTheWholeUnit) {
  const long long now = static_cast<long long>(std::time(nullptr));
  // Written two minutes ago: older than "-1m" but not than "-5min", and newer
  // than "-500ms" - which, read as 500 minutes, it was not.
  EXPECT_EQ(CRITICAL, verdict("written < -1m", aged(now - 120)));
  EXPECT_EQ(CRITICAL, verdict("written < -500ms", aged(now - 120)));
  EXPECT_EQ(OK, verdict("written < -5min", aged(now - 120)));
  EXPECT_TRUE(run("written < -5x", aged(now)).evaluation_failed);
}

TEST(DurationKeyword, SecondsPerUnitMatchesTheWholeUnit) {
  EXPECT_DOUBLE_EQ(0.001, duration_keyword::seconds_per_unit("ms"));
  EXPECT_DOUBLE_EQ(0.001, duration_keyword::seconds_per_unit("MS"));
  EXPECT_DOUBLE_EQ(1.0, duration_keyword::seconds_per_unit("s"));
  EXPECT_DOUBLE_EQ(60.0, duration_keyword::seconds_per_unit("m"));
  EXPECT_DOUBLE_EQ(60.0, duration_keyword::seconds_per_unit("min"));
  EXPECT_DOUBLE_EQ(3600.0, duration_keyword::seconds_per_unit("h"));
  EXPECT_DOUBLE_EQ(86400.0, duration_keyword::seconds_per_unit("d"));
  EXPECT_DOUBLE_EQ(604800.0, duration_keyword::seconds_per_unit("w"));
  EXPECT_DOUBLE_EQ(0.0, duration_keyword::seconds_per_unit("mo"));
  EXPECT_DOUBLE_EQ(0.0, duration_keyword::seconds_per_unit("x"));
}

TEST(ExpressionUnits, ByteUnitFactorMatchesTheWholeUnit) {
  long long f = 0;
  EXPECT_TRUE(str::format::byte_unit_factor("b", f));
  EXPECT_EQ(1, f);
  EXPECT_TRUE(str::format::byte_unit_factor("k", f));
  EXPECT_EQ(1024, f);
  EXPECT_TRUE(str::format::byte_unit_factor("KB", f));
  EXPECT_EQ(1024, f);
  EXPECT_TRUE(str::format::byte_unit_factor("MiB", f));
  EXPECT_EQ(1024 * 1024, f);
  EXPECT_TRUE(str::format::byte_unit_factor("t", f));
  EXPECT_EQ(1024LL * 1024 * 1024 * 1024, f);
  EXPECT_FALSE(str::format::byte_unit_factor("", f));
  EXPECT_FALSE(str::format::byte_unit_factor("x", f));
  EXPECT_FALSE(str::format::byte_unit_factor("kbb", f));
  EXPECT_FALSE(str::format::byte_unit_factor("%", f));
}
