// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_macros.hpp"

#include <gtest/gtest.h>

namespace json = boost::json;
using namespace icinga_agent;

namespace {
const json::object macros{{"a", "x"}, {"n", 5}, {"d", 80.0}, {"list", json::array{"p", "q"}}, {"flag", true}};

json::value expanded(const std::string &text) {
  const boost::optional<json::value> v = expand_macros(text, macros);
  EXPECT_TRUE(v.has_value()) << text;
  return v.value_or(json::value("<missing>"));
}
}  // namespace

TEST(IcingaAgentMacros, SoleMacroName) {
  EXPECT_EQ(sole_macro_name("$a$").value_or("-"), "a");
  EXPECT_FALSE(sole_macro_name("$$").has_value());
  EXPECT_FALSE(sole_macro_name("$a$ $b$").has_value());
  EXPECT_FALSE(sole_macro_name("x$a$").has_value());
  EXPECT_FALSE(sole_macro_name("$a$$b$").has_value());
  EXPECT_FALSE(sole_macro_name("").has_value());
}

TEST(IcingaAgentMacros, ASoleMacroKeepsItsType) {
  EXPECT_EQ(expanded("$a$"), json::value("x"));
  EXPECT_EQ(expanded("$n$"), json::value(5));
  EXPECT_EQ(expanded("$flag$"), json::value(true));
  EXPECT_EQ(expanded("$list$"), json::value(json::array{"p", "q"}));
}

TEST(IcingaAgentMacros, MacrosInsideTextBecomeText) {
  EXPECT_EQ(expanded("load > $n$"), json::value("load > 5"));
  EXPECT_EQ(expanded("used > $d$%"), json::value("used > 80%"));  // 80.0 from Icinga's JSON
  EXPECT_EQ(expanded("$a$-$a$"), json::value("x-x"));
  EXPECT_EQ(expanded("items: $list$"), json::value("items: p,q"));
}

TEST(IcingaAgentMacros, DoubleDollarIsALiteralDollar) {
  EXPECT_EQ(expanded("$$"), json::value("$"));
  EXPECT_EQ(expanded("cost $$5"), json::value("cost $5"));
  EXPECT_EQ(expanded("%(status): $${list}"), json::value("%(status): ${list}"));
}

TEST(IcingaAgentMacros, TextWithoutMacrosIsUnchanged) {
  EXPECT_EQ(expanded("plain"), json::value("plain"));
  EXPECT_EQ(expanded(""), json::value(""));
}

TEST(IcingaAgentMacros, AMissingMacroIsNone) {
  EXPECT_FALSE(expand_macros("$nope$", macros).has_value());
  EXPECT_FALSE(expand_macros("x $nope$ y", macros).has_value());
}

TEST(IcingaAgentMacros, AnUnterminatedMacroIsAnError) { EXPECT_THROW(expand_macros("x $a", macros), macro_syntax_error); }

TEST(IcingaAgentMacros, ExpandValueWalksArrays) {
  const auto v = expand_value(json::array{"$a$", "lit", "$n$"}, macros);
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(v.value(), json::value(json::array{"x", "lit", 5}));
  EXPECT_FALSE(expand_value(json::array{"$a$", "$nope$"}, macros).has_value());
  EXPECT_EQ(expand_value(json::value(3), macros).value(), json::value(3));
}
