// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_protocol.hpp"

#include <gtest/gtest.h>

#include <boost/json.hpp>
#include <string>
#include <vector>

namespace json = boost::json;

typedef std::vector<std::string> strings;

// ---- Path ------------------------------------------------------------------

TEST(NcpaPath, EmptyPathIsTheRoot) {
  EXPECT_EQ(ncpa::split_path(""), strings());
  EXPECT_EQ(ncpa::split_path("/"), strings());
}

TEST(NcpaPath, SplitsOnSlashAndDropsEmptySegments) {
  // check_ncpa always puts a '/' after the metric, even with no arguments.
  EXPECT_EQ(ncpa::split_path("plugins/check_cpu/"), (strings{"plugins", "check_cpu"}));
  EXPECT_EQ(ncpa::split_path("plugins//check_cpu"), (strings{"plugins", "check_cpu"}));
  EXPECT_EQ(ncpa::split_path("cpu/percent"), (strings{"cpu", "percent"}));
}

TEST(NcpaPath, DecodesEachSegmentAfterSplitting) {
  // check_ncpa quotes every -a token with safe='' - an escaped slash inside an
  // argument must stay inside that argument.
  EXPECT_EQ(ncpa::split_path("plugins/check_files/path%3D%2Ftmp/warning%3Dcount%3E5"), (strings{"plugins", "check_files", "path=/tmp", "warning=count>5"}));
}

TEST(NcpaPath, KeepsAPlusInAPathSegment) { EXPECT_EQ(ncpa::split_path("plugins/a+b"), (strings{"plugins", "a+b"})); }

TEST(NcpaPath, SpacesAndMountEncoding) {
  EXPECT_EQ(ncpa::split_path("plugins/check_cpu/warning%3Dload%20%3E%2080"), (strings{"plugins", "check_cpu", "warning=load > 80"}));
  // NCPA writes mount points with '|' for the separator, quoted as %7C.
  EXPECT_EQ(ncpa::split_path("disk/logical/C%3A%7C"), (strings{"disk", "logical", "C:|"}));
}

TEST(NcpaDecode, MalformedEscapesPassThrough) {
  EXPECT_EQ(ncpa::percent_decode("100%", false), "100%");
  EXPECT_EQ(ncpa::percent_decode("%zz", false), "%zz");
  EXPECT_EQ(ncpa::percent_decode("%4", false), "%4");
  EXPECT_EQ(ncpa::percent_decode("%41", false), "A");
  EXPECT_EQ(ncpa::percent_decode("a+b", true), "a b");
}

// ---- Form ------------------------------------------------------------------

TEST(NcpaForm, ParsesWhatCheckNcpaSends) {
  // urlencode() of check_ncpa's argument dict; None values are left out.
  const ncpa::form_vector f = ncpa::parse_form("token=s3cr%26t&warning=80&critical=90&delta=False&check=1");
  EXPECT_EQ(ncpa::form_value(f, "token"), "s3cr&t");
  EXPECT_EQ(ncpa::form_value(f, "check"), "1");
  EXPECT_EQ(ncpa::form_value(f, "delta"), "False");
  EXPECT_EQ(ncpa::form_value(f, "units", "none"), "none");
  EXPECT_TRUE(ncpa::form_has(f, "warning"));
  EXPECT_FALSE(ncpa::form_has(f, "units"));
}

TEST(NcpaForm, FirstValueWinsAndAllValuesAreKept) {
  const ncpa::form_vector f = ncpa::parse_form("args=-w+10&args=-c%2020&flag&token=");
  EXPECT_EQ(ncpa::form_value(f, "args"), "-w 10");
  EXPECT_EQ(ncpa::form_values(f, "args"), (strings{"-w 10", "-c 20"}));
  EXPECT_TRUE(ncpa::form_has(f, "flag"));
  EXPECT_TRUE(ncpa::form_has(f, "token"));
  EXPECT_EQ(ncpa::form_value(f, "token", "fallback"), "");
}

TEST(NcpaForm, Truthiness) {
  EXPECT_TRUE(ncpa::is_truthy("1"));
  EXPECT_TRUE(ncpa::is_truthy("true"));
  EXPECT_TRUE(ncpa::is_truthy("yes"));
  // check_ncpa sends delta=False literally when -d is not given.
  EXPECT_FALSE(ncpa::is_truthy("False"));
  EXPECT_FALSE(ncpa::is_truthy("0"));
  EXPECT_FALSE(ncpa::is_truthy(""));
  EXPECT_FALSE(ncpa::is_truthy(" off "));
}

// ---- Arguments -------------------------------------------------------------

TEST(NcpaArgs, SplitsLikeAShell) {
  EXPECT_EQ(ncpa::split_args("-w 10 -c 20"), (strings{"-w", "10", "-c", "20"}));
  EXPECT_EQ(ncpa::split_args("  spaced   out  "), (strings{"spaced", "out"}));
  EXPECT_EQ(ncpa::split_args("\"filter=used > 80\" 'top-syntax=${list}'"), (strings{"filter=used > 80", "top-syntax=${list}"}));
  EXPECT_EQ(ncpa::split_args("a\\ b c"), (strings{"a b", "c"}));
  EXPECT_EQ(ncpa::split_args("\"say \\\"hi\\\"\""), (strings{"say \"hi\""}));
  EXPECT_EQ(ncpa::split_args("''"), (strings{""}));
  EXPECT_EQ(ncpa::split_args(""), strings());
}

TEST(NcpaArgs, UnterminatedQuoteKeepsWhatItCollected) { EXPECT_EQ(ncpa::split_args("\"open quote"), (strings{"open quote"})); }

// ---- Token -----------------------------------------------------------------

TEST(NcpaToken, ConstantTimeEquals) {
  EXPECT_TRUE(ncpa::constant_time_equals("secret", "secret"));
  EXPECT_FALSE(ncpa::constant_time_equals("secreT", "secret"));
  EXPECT_FALSE(ncpa::constant_time_equals("secret-and-more", "secret"));
  EXPECT_FALSE(ncpa::constant_time_equals("secre", "secret"));
  EXPECT_FALSE(ncpa::constant_time_equals("", "secret"));
  EXPECT_FALSE(ncpa::constant_time_equals("x", ""));
  EXPECT_TRUE(ncpa::constant_time_equals("", ""));
}

TEST(NcpaToken, NothingIsAcceptedWithoutAConfiguredToken) {
  EXPECT_EQ(ncpa::check_token("", "", ""), ncpa::token_result::not_configured);
  EXPECT_EQ(ncpa::check_token("mytoken", "", "mytoken"), ncpa::token_result::not_configured);
}

TEST(NcpaToken, PrimaryAndBackup) {
  EXPECT_EQ(ncpa::check_token("one", "one", ""), ncpa::token_result::accepted);
  EXPECT_EQ(ncpa::check_token("two", "one", "two"), ncpa::token_result::accepted);
  EXPECT_EQ(ncpa::check_token("three", "one", "two"), ncpa::token_result::rejected);
  EXPECT_EQ(ncpa::check_token("two", "one", ""), ncpa::token_result::rejected);
}

TEST(NcpaToken, AnEmptyTokenNeverMatchesAnEmptyBackup) {
  EXPECT_EQ(ncpa::check_token("", "one", ""), ncpa::token_result::rejected);
  EXPECT_EQ(ncpa::check_token("", "one", "two"), ncpa::token_result::rejected);
}

// ---- Plugin policy ---------------------------------------------------------

TEST(NcpaPolicy, Any) {
  ncpa::plugin_policy p;
  std::string error;
  ASSERT_TRUE(ncpa::plugin_policy::parse(" ANY ", p, error));
  EXPECT_TRUE(p.allows("check_cpu", "CheckSystem"));
  EXPECT_TRUE(p.allows("my_script", "CheckExternalScripts"));
  EXPECT_EQ(p.to_string(), "any");
}

TEST(NcpaPolicy, ScriptsOnly) {
  ncpa::plugin_policy p;
  std::string error;
  ASSERT_TRUE(ncpa::plugin_policy::parse("scripts", p, error));
  EXPECT_TRUE(p.allows("my_script", "CheckExternalScripts"));
  EXPECT_TRUE(p.allows("my_script", "checkexternalscripts"));
  EXPECT_FALSE(p.allows("check_cpu", "CheckSystem"));
  EXPECT_FALSE(p.allows("check_cpu", ""));
}

TEST(NcpaPolicy, ListIsCaseInsensitiveAndTrimmed) {
  ncpa::plugin_policy p;
  std::string error;
  ASSERT_TRUE(ncpa::plugin_policy::parse("check_cpu, Check_Memory ,,", p, error));
  EXPECT_TRUE(p.allows("check_cpu", "CheckSystem"));
  EXPECT_TRUE(p.allows("CHECK_MEMORY", "CheckSystem"));
  EXPECT_FALSE(p.allows("check_uptime", "CheckSystem"));
  EXPECT_EQ(p.to_string(), "check_cpu,check_memory");
}

TEST(NcpaPolicy, AnEmptyListIsRefused) {
  ncpa::plugin_policy p;
  std::string error;
  EXPECT_FALSE(ncpa::plugin_policy::parse("", p, error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(ncpa::plugin_policy::parse(" , ", p, error));
}

// ---- Output ----------------------------------------------------------------

TEST(NcpaOutput, MessageAndPerfdata) {
  EXPECT_EQ(ncpa::nagios_output("OK: fine", ""), "OK: fine");
  EXPECT_EQ(ncpa::nagios_output("OK: fine", "'load'=3%;80;90"), "OK: fine|'load'=3%;80;90");
  EXPECT_EQ(ncpa::nagios_output("line one\r\nline two\r\n", ""), "line one\nline two");
}

TEST(NcpaBody, Error) {
  const json::value v = json::parse(ncpa::error_body("Incorrect credentials given."));
  EXPECT_EQ(v.at("error").as_string(), "Incorrect credentials given.");
  EXPECT_EQ(v.as_object().size(), 1u);
}

TEST(NcpaBody, Check) {
  const json::value v = json::parse(ncpa::check_body(1, "WARNING: \"quoted\" | 'x'=1"));
  EXPECT_EQ(v.at("returncode").as_int64(), 1);
  EXPECT_EQ(v.at("stdout").as_string(), "WARNING: \"quoted\" | 'x'=1");
}

TEST(NcpaBody, List) {
  const json::value v = json::parse(ncpa::list_body("plugins", {"check_cpu", "check_uptime"}));
  const json::array &a = v.at("plugins").as_array();
  ASSERT_EQ(a.size(), 2u);
  EXPECT_EQ(a[0].as_string(), "check_cpu");
  EXPECT_EQ(a[1].as_string(), "check_uptime");
  EXPECT_EQ(json::parse(ncpa::list_body("plugins", {})).at("plugins").as_array().size(), 0u);
}

TEST(NcpaBody, MissingNodeMatchesNcpa) {
  const json::value v = json::parse(ncpa::missing_node_body("/api/plugins/nope", "plugin", "nope"));
  const json::object &e = v.at("error").as_object();
  EXPECT_EQ(e.at("path").as_string(), "/api/plugins/nope");
  EXPECT_EQ(e.at("code").as_int64(), 100);
  EXPECT_EQ(e.at("message").as_string(), "The plugin requested does not exist.");
  EXPECT_EQ(e.at("plugin").as_string(), "nope");
}

TEST(NcpaBody, MissingNodeInCheckModeIsUnknown) {
  const json::value v = json::parse(ncpa::missing_node_check_body("node", "a|b"));
  EXPECT_EQ(v.at("returncode").as_int64(), 3);
  // A '|' would start the perfdata on the Nagios side.
  EXPECT_EQ(v.at("stdout").as_string(), "UNKNOWN: The node (a/b) requested does not exist.");
}
