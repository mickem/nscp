/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include <parsers/expression/expression.hpp>
#include <string>
#include <vector>

// ======================================================================
// Helpers
// ======================================================================

using entry = parsers::simple_expression::entry;
using result_type = parsers::simple_expression::result_type;

static bool parse(const std::string& str, result_type& v) {
  return parsers::simple_expression::parse(str, v);
}

static void expect_literal(const entry& e, const std::string& expected_name) {
  EXPECT_FALSE(e.is_variable) << "expected literal, got variable: " << e.name;
  EXPECT_EQ(expected_name, e.name);
}

static void expect_variable(const entry& e, const std::string& expected_name) {
  EXPECT_TRUE(e.is_variable) << "expected variable, got literal: " << e.name;
  EXPECT_EQ(expected_name, e.name);
}

// ======================================================================
// Empty and plain text
// ======================================================================

TEST(ExpressionParser, EmptyStringReturnsNoEntries) {
  result_type v;
  EXPECT_TRUE(parse("", v));
  EXPECT_TRUE(v.empty());
}

TEST(ExpressionParser, PlainTextReturnsOneLiteral) {
  result_type v;
  EXPECT_TRUE(parse("hello world", v));
  ASSERT_EQ(1u, v.size());
  expect_literal(v[0], "hello world");
}

TEST(ExpressionParser, PlainTextWithSpecialChars) {
  result_type v;
  EXPECT_TRUE(parse("abc/def.txt:123", v));
  ASSERT_EQ(1u, v.size());
  expect_literal(v[0], "abc/def.txt:123");
}

// ======================================================================
// Dollar-brace variables: ${...}
// ======================================================================

TEST(ExpressionParser, SingleDollarBraceVariable) {
  result_type v;
  EXPECT_TRUE(parse("${host}", v));
  ASSERT_EQ(1u, v.size());
  expect_variable(v[0], "host");
}

TEST(ExpressionParser, DollarBraceVariableWithSurroundingText) {
  result_type v;
  EXPECT_TRUE(parse("prefix_${name}_suffix", v));
  ASSERT_EQ(3u, v.size());
  expect_literal(v[0], "prefix_");
  expect_variable(v[1], "name");
  expect_literal(v[2], "_suffix");
}

TEST(ExpressionParser, MultipleDollarBraceVariables) {
  result_type v;
  EXPECT_TRUE(parse("${a}${b}", v));
  ASSERT_EQ(2u, v.size());
  expect_variable(v[0], "a");
  expect_variable(v[1], "b");
}

TEST(ExpressionParser, DollarBraceVariableWithDots) {
  result_type v;
  EXPECT_TRUE(parse("${host.name.fqdn}", v));
  ASSERT_EQ(1u, v.size());
  expect_variable(v[0], "host.name.fqdn");
}

// ======================================================================
// Percent-paren variables: %(...)
// ======================================================================

TEST(ExpressionParser, SinglePercentParenVariable) {
  result_type v;
  EXPECT_TRUE(parse("%(host)", v));
  ASSERT_EQ(1u, v.size());
  expect_variable(v[0], "host");
}

TEST(ExpressionParser, PercentParenVariableWithSurroundingText) {
  result_type v;
  EXPECT_TRUE(parse("prefix_%(name)_suffix", v));
  ASSERT_EQ(3u, v.size());
  expect_literal(v[0], "prefix_");
  expect_variable(v[1], "name");
  expect_literal(v[2], "_suffix");
}

TEST(ExpressionParser, MultiplePercentParenVariables) {
  result_type v;
  EXPECT_TRUE(parse("%(a)%(b)", v));
  ASSERT_EQ(2u, v.size());
  expect_variable(v[0], "a");
  expect_variable(v[1], "b");
}

TEST(ExpressionParser, PercentParenVariableWithDots) {
  result_type v;
  EXPECT_TRUE(parse("%(host.name.fqdn)", v));
  ASSERT_EQ(1u, v.size());
  expect_variable(v[0], "host.name.fqdn");
}

// ======================================================================
// Mixed variable styles
// ======================================================================

TEST(ExpressionParser, MixedDollarBraceAndPercentParen) {
  result_type v;
  EXPECT_TRUE(parse("${a}_%(b)", v));
  ASSERT_EQ(3u, v.size());
  expect_variable(v[0], "a");
  expect_literal(v[1], "_");
  expect_variable(v[2], "b");
}

TEST(ExpressionParser, MixedPercentParenAndDollarBrace) {
  result_type v;
  EXPECT_TRUE(parse("%(first)-${second}", v));
  ASSERT_EQ(3u, v.size());
  expect_variable(v[0], "first");
  expect_literal(v[1], "-");
  expect_variable(v[2], "second");
}

// ======================================================================
// Complex expressions
// ======================================================================

TEST(ExpressionParser, ComplexExpressionMultipleVariablesAndText) {
  result_type v;
  EXPECT_TRUE(parse("Hello ${user}, your score is %(score)!", v));
  ASSERT_EQ(5u, v.size());
  expect_literal(v[0], "Hello ");
  expect_variable(v[1], "user");
  expect_literal(v[2], ", your score is ");
  expect_variable(v[3], "score");
  expect_literal(v[4], "!");
}

TEST(ExpressionParser, VariableAtStart) {
  result_type v;
  EXPECT_TRUE(parse("${start}rest", v));
  ASSERT_EQ(2u, v.size());
  expect_variable(v[0], "start");
  expect_literal(v[1], "rest");
}

TEST(ExpressionParser, VariableAtEnd) {
  result_type v;
  EXPECT_TRUE(parse("rest${end}", v));
  ASSERT_EQ(2u, v.size());
  expect_literal(v[0], "rest");
  expect_variable(v[1], "end");
}

TEST(ExpressionParser, OnlyVariables) {
  result_type v;
  EXPECT_TRUE(parse("${a}%(b)${c}", v));
  ASSERT_EQ(3u, v.size());
  expect_variable(v[0], "a");
  expect_variable(v[1], "b");
  expect_variable(v[2], "c");
}

// ======================================================================
// Variable names with special characters
// ======================================================================

TEST(ExpressionParser, DollarBraceVariableWithSlash) {
  result_type v;
  EXPECT_TRUE(parse("${path/to/key}", v));
  ASSERT_EQ(1u, v.size());
  expect_variable(v[0], "path/to/key");
}

TEST(ExpressionParser, DollarBraceVariableWithSpaces) {
  result_type v;
  EXPECT_TRUE(parse("${my variable}", v));
  ASSERT_EQ(1u, v.size());
  expect_variable(v[0], "my variable");
}

TEST(ExpressionParser, PercentParenVariableWithUnderscore) {
  result_type v;
  EXPECT_TRUE(parse("%(my_var)", v));
  ASSERT_EQ(1u, v.size());
  expect_variable(v[0], "my_var");
}

// ======================================================================
// Edge cases
// ======================================================================

TEST(ExpressionParser, DollarWithoutBraceIsLiteral) {
  result_type v;
  EXPECT_TRUE(parse("cost is $100", v));
  ASSERT_EQ(1u, v.size());
  expect_literal(v[0], "cost is $100");
}

TEST(ExpressionParser, PercentWithoutParenIsLiteral) {
  result_type v;
  EXPECT_TRUE(parse("100%done", v));
  ASSERT_EQ(1u, v.size());
  expect_literal(v[0], "100%done");
}

TEST(ExpressionParser, SingleCharLiteralBetweenVariables) {
  result_type v;
  EXPECT_TRUE(parse("${x}.${y}", v));
  ASSERT_EQ(3u, v.size());
  expect_variable(v[0], "x");
  expect_literal(v[1], ".");
  expect_variable(v[2], "y");
}

TEST(ExpressionParser, NestedLookingDollarBraceIsNotNested) {
  result_type v;
  // With fallback rule: "${" starts variable_rule_d, but "outer${inner" contains no '}',
  // so variable_rule_d fails. The fallback rule consumes "$" as literal, then "{" as literal,
  // then the rest is parsed normally. The net result is all characters consumed as literals/variables.
  EXPECT_TRUE(parse("${outer${inner}}", v));
  // TODO: Should this be an error?
  ASSERT_EQ(2u, v.size());
  expect_variable(v[0], "outer${inner");
  expect_literal(v[1], "}");
}

TEST(ExpressionParser, AdjacentDollarBraceAndPercentParen) {
  result_type v;
  EXPECT_TRUE(parse("${a}%(b)${c}%(d)", v));
  ASSERT_EQ(4u, v.size());
  expect_variable(v[0], "a");
  expect_variable(v[1], "b");
  expect_variable(v[2], "c");
  expect_variable(v[3], "d");
}

// ======================================================================
// Unclosed / malformed variables (fallback handling)
// ======================================================================

TEST(ExpressionParser, UnclosedDollarBraceIsTreatedAsLiteral) {
  result_type v;
  EXPECT_TRUE(parse("hello ${unclosed", v));
  // The fallback rule consumes "${" character by character as literals
  ASSERT_EQ(3u, v.size());
  expect_literal(v[0], "hello ");
  expect_literal(v[1], "$");
  expect_literal(v[2], "{unclosed");
}

TEST(ExpressionParser, UnclosedPercentParenIsTreatedAsLiteral) {
  result_type v;
  EXPECT_TRUE(parse("hello %(unclosed", v));
  ASSERT_EQ(3u, v.size());
  expect_literal(v[0], "hello ");
  expect_literal(v[1], "%");
  expect_literal(v[2], "(unclosed");
}

TEST(ExpressionParser, EmptyDollarBraceVariable) {
  result_type v;
  // "${}" has no content between delimiters — the '+' operator requires at least one char
  // so variable_rule_d fails and fallback consumes "$", "{", "}" as literals
  EXPECT_TRUE(parse("${}", v));
  ASSERT_EQ(2u, v.size());
  expect_literal(v[0], "$");
  expect_literal(v[1], "{}");
}

TEST(ExpressionParser, EmptyPercentParenVariable) {
  result_type v;
  // "%()" — same as above for percent-paren style
  EXPECT_TRUE(parse("%()", v));
  ASSERT_EQ(2u, v.size());
  expect_literal(v[0], "%");
  expect_literal(v[1], "()");
}

TEST(ExpressionParser, LoneDollarBraceOpener) {
  result_type v;
  EXPECT_TRUE(parse("${", v));
  ASSERT_EQ(2u, v.size());
  expect_literal(v[0], "$");
  expect_literal(v[1], "{");
}

TEST(ExpressionParser, LonePercentParenOpener) {
  result_type v;
  EXPECT_TRUE(parse("%(", v));
  ASSERT_EQ(2u, v.size());
  expect_literal(v[0], "%");
  expect_literal(v[1], "(");
}

TEST(ExpressionParser, ClosingBraceAloneIsLiteral) {
  result_type v;
  EXPECT_TRUE(parse("}", v));
  ASSERT_EQ(1u, v.size());
  expect_literal(v[0], "}");
}

TEST(ExpressionParser, ClosingParenAloneIsLiteral) {
  result_type v;
  EXPECT_TRUE(parse(")", v));
  ASSERT_EQ(1u, v.size());
  expect_literal(v[0], ")");
}

TEST(ExpressionParser, UnclosedFollowedByValidVariable) {
  result_type v;
  EXPECT_TRUE(parse("${bad${good}", v));
  ASSERT_EQ(1u, v.size());
  expect_variable(v[0], "bad${good");
}

TEST(ExpressionParser, SpecialCharMixFromOldTests) {
  result_type v;
  EXPECT_TRUE(parse("Hello$}Wo%)rld}$Fo%o}Ba)r%$%En%%)kkk(uuu)kkk{yyyy}d", v));
  // All non-variable text should be consumed as literals
  std::string reconstructed;
  for (const auto& e : v) {
    EXPECT_FALSE(e.is_variable);
    reconstructed += e.name;
  }
  EXPECT_EQ("Hello$}Wo%)rld}$Fo%o}Ba)r%$%En%%)kkk(uuu)kkk{yyyy}d", reconstructed);
}

// ======================================================================
// Real-world–like patterns
// ======================================================================

TEST(ExpressionParser, NscpStyleCommand) {
  result_type v;
  EXPECT_TRUE(parse("check_cpu warn=${warn} crit=${crit}", v));
  ASSERT_EQ(4u, v.size());
  expect_literal(v[0], "check_cpu warn=");
  expect_variable(v[1], "warn");
  expect_literal(v[2], " crit=");
  expect_variable(v[3], "crit");
}

TEST(ExpressionParser, PathWithVariable) {
  result_type v;
  EXPECT_TRUE(parse("/usr/local/${app}/bin", v));
  ASSERT_EQ(3u, v.size());
  expect_literal(v[0], "/usr/local/");
  expect_variable(v[1], "app");
  expect_literal(v[2], "/bin");
}

TEST(ExpressionParser, UrlTemplate) {
  result_type v;
  EXPECT_TRUE(parse("https://%(host):%(port)/api/%(version)", v));
  ASSERT_EQ(6u, v.size());
  expect_literal(v[0], "https://");
  expect_variable(v[1], "host");
  expect_literal(v[2], ":");
  expect_variable(v[3], "port");
  expect_literal(v[4], "/api/");
  expect_variable(v[5], "version");
}

TEST(ExpressionParser, ComplexDollarBraceWithBracesInText) {
  result_type v;
  EXPECT_TRUE(parse("HelloWorld${foobar}MoreData${test}-}}{{-${test{{2}", v));
  ASSERT_EQ(6u, v.size());
  expect_literal(v[0], "HelloWorld");
  expect_variable(v[1], "foobar");
  expect_literal(v[2], "MoreData");
  expect_variable(v[3], "test");
  expect_literal(v[4], "-}}{{-");
  expect_variable(v[5], "test{{2");
}

TEST(ExpressionParser, ComplexPercentParenWithParensInText) {
  result_type v;
  EXPECT_TRUE(parse("HelloWorld%(foobar)MoreData%(test)-))((-%(test((2)", v));
  ASSERT_EQ(6u, v.size());
  expect_literal(v[0], "HelloWorld");
  expect_variable(v[1], "foobar");
  expect_literal(v[2], "MoreData");
  expect_variable(v[3], "test");
  expect_literal(v[4], "-))((-");
  expect_variable(v[5], "test((2");
}

