// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <cstring>
#include <nscapi/nscapi_helper.hpp>
#include <string>

namespace rc = NSCAPI::query_return_codes;
namespace ll = NSCAPI::log_level;
using nscapi::plugin_helper::maxState;
using nscapi::plugin_helper::translateReturn;

// ---------------------------------------------------------------------------
// report: the Scheduler's `report` key - which results reach the channel.
// ---------------------------------------------------------------------------

TEST(Report, ParseSingleKeywords) {
  const unsigned int all = nscapi::report::parse("all");
  EXPECT_EQ(nscapi::report::parse("ok,warn,crit,unknown"), all);
  for (const char *crit : {"error", "err", "critical", "crit"}) EXPECT_EQ(nscapi::report::parse(crit), nscapi::report::parse("crit")) << crit;
  EXPECT_EQ(nscapi::report::parse("warning"), nscapi::report::parse("warn"));
  EXPECT_EQ(nscapi::report::parse(""), 0u);
  EXPECT_EQ(nscapi::report::parse("bogus"), 0u);
}

// The natural way to write a list - with spaces, in any case - must not
// silently drop entries.
TEST(Report, ParseTrimsAndIgnoresCase) {
  EXPECT_EQ(nscapi::report::parse("critical, warning"), nscapi::report::parse("critical,warning"));
  EXPECT_EQ(nscapi::report::parse(" OK , Unknown "), nscapi::report::parse("ok,unknown"));
  EXPECT_EQ(nscapi::report::parse("ALL"), nscapi::report::parse("all"));
}

TEST(Report, Matches) {
  const unsigned int problems = nscapi::report::parse("crit,warn,unknown");
  EXPECT_FALSE(nscapi::report::matches(problems, rc::returnOK));
  EXPECT_TRUE(nscapi::report::matches(problems, rc::returnWARN));
  EXPECT_TRUE(nscapi::report::matches(problems, rc::returnCRIT));
  EXPECT_TRUE(nscapi::report::matches(problems, rc::returnUNKNOWN));

  const unsigned int ok_only = nscapi::report::parse("ok");
  EXPECT_TRUE(nscapi::report::matches(ok_only, rc::returnOK));
  EXPECT_FALSE(nscapi::report::matches(ok_only, rc::returnCRIT));

  // A code outside the four Nagios states is always passed on, so a broken
  // result is not swallowed by the filter.
  EXPECT_TRUE(nscapi::report::matches(0, 42));
  EXPECT_TRUE(nscapi::report::matches(ok_only, -1));
}

TEST(Report, ToStringRoundTrips) {
  EXPECT_EQ(nscapi::report::to_string(0), "<none>");
  EXPECT_EQ(nscapi::report::to_string(nscapi::report::parse("ok")), "ok");
  EXPECT_EQ(nscapi::report::to_string(nscapi::report::parse("warn")), "warn");
  EXPECT_EQ(nscapi::report::to_string(nscapi::report::parse("unknown")), "unknown");
  EXPECT_EQ(nscapi::report::to_string(nscapi::report::parse("all")), "ok,crit,warn,unknown");
  EXPECT_EQ(nscapi::report::to_string(nscapi::report::parse("warn,unknown")), "warn,unknown");
  for (const char *s : {"ok", "crit", "warn,unknown", "ok,crit,warn,unknown"}) {
    EXPECT_EQ(nscapi::report::parse(nscapi::report::to_string(nscapi::report::parse(s))), nscapi::report::parse(s)) << s;
  }
}

// ---------------------------------------------------------------------------
// logging
// ---------------------------------------------------------------------------

TEST(Logging, ParseKnownLevels) {
  EXPECT_EQ(nscapi::logging::parse("all"), ll::trace);
  EXPECT_EQ(nscapi::logging::parse("trace"), ll::trace);
  EXPECT_EQ(nscapi::logging::parse("debug"), ll::debug);
  EXPECT_EQ(nscapi::logging::parse("info"), ll::info);
  EXPECT_EQ(nscapi::logging::parse("warning"), ll::warning);
  EXPECT_EQ(nscapi::logging::parse("error"), ll::error);
  EXPECT_EQ(nscapi::logging::parse("critical"), ll::critical);
  EXPECT_EQ(nscapi::logging::parse("off"), ll::off);
  EXPECT_EQ(nscapi::logging::parse("ERROR"), ll::error);
  EXPECT_EQ(nscapi::logging::parse("nonsense"), ll::unknown);
  EXPECT_EQ(nscapi::logging::parse(""), ll::unknown);
}

TEST(Logging, ToStringRoundTrips) {
  for (const int level : {ll::trace, ll::debug, ll::info, ll::warning, ll::error, ll::critical, ll::off}) {
    EXPECT_EQ(nscapi::logging::parse(nscapi::logging::to_string(level)), level) << level;
  }
  EXPECT_EQ(nscapi::logging::to_string(ll::unknown), "unknown");
  EXPECT_EQ(nscapi::logging::to_string(12345), "unknown");
}

TEST(Logging, MatchesIsAtOrBelowTheLevel) {
  EXPECT_TRUE(nscapi::logging::matches(ll::warning, ll::error));
  EXPECT_TRUE(nscapi::logging::matches(ll::warning, ll::warning));
  EXPECT_FALSE(nscapi::logging::matches(ll::warning, ll::info));
  EXPECT_FALSE(nscapi::logging::matches(ll::off, ll::critical));
}

// ---------------------------------------------------------------------------
// plugin_helper
// ---------------------------------------------------------------------------

TEST(PluginHelper, TranslateReturn) {
  EXPECT_EQ(translateReturn(rc::returnOK), "OK");
  EXPECT_EQ(translateReturn(rc::returnWARN), "WARNING");
  EXPECT_EQ(translateReturn(rc::returnCRIT), "CRITICAL");
  EXPECT_EQ(translateReturn(rc::returnUNKNOWN), "UNKNOWN");
  EXPECT_EQ(translateReturn(7), "BAD_CODE: 7");

  EXPECT_EQ(translateReturn(std::string("OK")), rc::returnOK);
  EXPECT_EQ(translateReturn(std::string("ok")), rc::returnOK);
  EXPECT_EQ(translateReturn(std::string("WARNING")), rc::returnWARN);
  EXPECT_EQ(translateReturn(std::string("critical")), rc::returnCRIT);
  EXPECT_EQ(translateReturn(std::string("UNKNOWN")), rc::returnUNKNOWN);
  EXPECT_EQ(translateReturn(std::string("whatever")), rc::returnUNKNOWN);
}

// OK < WARNING < CRITICAL < UNKNOWN. A code outside the four ranks below all
// of them, and two such codes give UNKNOWN.
TEST(PluginHelper, MaxState) {
  EXPECT_EQ(maxState(rc::returnOK, rc::returnOK), rc::returnOK);
  EXPECT_EQ(maxState(rc::returnOK, rc::returnWARN), rc::returnWARN);
  EXPECT_EQ(maxState(rc::returnCRIT, rc::returnWARN), rc::returnCRIT);
  EXPECT_EQ(maxState(rc::returnCRIT, rc::returnUNKNOWN), rc::returnUNKNOWN);
  EXPECT_EQ(maxState(rc::returnUNKNOWN, rc::returnOK), rc::returnUNKNOWN);
  EXPECT_EQ(maxState(42, 43), rc::returnUNKNOWN);
  EXPECT_EQ(maxState(42, rc::returnOK), rc::returnOK);
}

TEST(PluginHelper, Escalation) {
  int code = rc::returnOK;
  nscapi::plugin_helper::escalteReturnCodeToWARN(code);
  EXPECT_EQ(code, rc::returnWARN);
  code = rc::returnCRIT;
  nscapi::plugin_helper::escalteReturnCodeToWARN(code);
  EXPECT_EQ(code, rc::returnCRIT);
  code = rc::returnOK;
  nscapi::plugin_helper::escalteReturnCodeToCRIT(code);
  EXPECT_EQ(code, rc::returnCRIT);

  for (const int start : {rc::returnWARN, rc::returnCRIT, rc::returnUNKNOWN}) {
    code = start;
    nscapi::plugin_helper::escalateReturnCodeToUNKNOWN(code);
    EXPECT_EQ(code, start);
  }
  code = rc::returnOK;
  nscapi::plugin_helper::escalateReturnCodeToUNKNOWN(code);
  EXPECT_EQ(code, rc::returnUNKNOWN);
}

TEST(PluginHelper, IsNagiosReturnCode) {
  for (const int c : {rc::returnOK, rc::returnWARN, rc::returnCRIT, rc::returnUNKNOWN}) {
    EXPECT_TRUE(nscapi::plugin_helper::isNagiosReturnCode(c));
    EXPECT_TRUE(nscapi::plugin_helper::isMyNagiosReturn(c));
  }
  EXPECT_FALSE(nscapi::plugin_helper::isNagiosReturnCode(4));
  EXPECT_FALSE(nscapi::plugin_helper::isMyNagiosReturn(-1));
}

TEST(PluginHelper, WrapReturnString) {
  char buffer[8];
  std::memset(buffer, 'x', sizeof(buffer));
  EXPECT_EQ(nscapi::plugin_helper::wrapReturnString(buffer, sizeof(buffer), "hello", 17), 17);
  EXPECT_STREQ(buffer, "hello");
  // The string plus its terminator must fit.
  EXPECT_EQ(nscapi::plugin_helper::wrapReturnString(buffer, sizeof(buffer), "1234567", 1), 1);
  EXPECT_STREQ(buffer, "1234567");
  EXPECT_EQ(nscapi::plugin_helper::wrapReturnString(buffer, sizeof(buffer), "12345678", 1), NSCAPI::isInvalidBufferLen);
}
