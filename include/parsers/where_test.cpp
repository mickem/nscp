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

#include <map>
#include <memory>
#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <string>

using namespace parsers::where;

// ======================================================================
// Mock evaluation context
// ======================================================================

struct mock_evaluation_context final : evaluation_context_interface {
  std::string error_;
  std::string warn_;
  bool debug_enabled_ = false;

  bool has_error() const override { return !error_.empty(); }
  std::string get_error() const override { return error_; }
  void error(std::string msg) override {
    if (!error_.empty()) error_ += ", ";
    error_ += msg;
  }
  bool has_warn() const override { return !warn_.empty(); }
  std::string get_warn() const override { return warn_; }
  void warn(std::string msg) override { warn_ += msg; }
  void clear() override {
    error_.clear();
    warn_.clear();
  }
  void enable_debug(bool enable) override { debug_enabled_ = enable; }
  bool debug_enabled() override { return debug_enabled_; }
  std::string get_debug() const override { return ""; }
  void debug(object_match) override {}
};

// ======================================================================
// Mock object_converter
// ======================================================================

struct mock_object_converter final : object_converter_interface {
  std::string error_;
  std::string warn_;
  bool debug_enabled_ = false;

  bool has_error() const override { return !error_.empty(); }
  std::string get_error() const override { return error_; }
  void error(std::string msg) override {
    if (!error_.empty()) error_ += ", ";
    error_ += msg;
  }
  bool has_warn() const override { return !warn_.empty(); }
  std::string get_warn() const override { return warn_; }
  void warn(std::string msg) override { warn_ += msg; }
  void clear() override {
    error_.clear();
    warn_.clear();
  }
  void enable_debug(bool enable) override { debug_enabled_ = enable; }
  bool debug_enabled() override { return debug_enabled_; }
  std::string get_debug() const override { return ""; }
  void debug(object_match) override {}

  bool can_convert(value_type, value_type) override { return false; }
  bool can_convert(std::string, std::shared_ptr<any_node>, value_type) override { return false; }
  std::shared_ptr<binary_function_impl> create_converter(std::string, std::shared_ptr<any_node>, value_type) override { return nullptr; }
};

// ======================================================================
// Mock object_factory
// ======================================================================

struct mock_object_factory final : object_factory_interface {
  std::string error_;
  std::string warn_;
  bool debug_enabled_ = false;
  std::map<std::string, node_type> variables_;

  bool has_error() const override { return !error_.empty(); }
  std::string get_error() const override { return error_; }
  void error(std::string msg) override {
    if (!error_.empty()) error_ += ", ";
    error_ += msg;
  }
  bool has_warn() const override { return !warn_.empty(); }
  std::string get_warn() const override { return warn_; }
  void warn(std::string msg) override { warn_ += msg; }
  void clear() override {
    error_.clear();
    warn_.clear();
  }
  void enable_debug(bool enable) override { debug_enabled_ = enable; }
  bool debug_enabled() override { return debug_enabled_; }
  std::string get_debug() const override { return ""; }
  void debug(object_match) override {}

  bool can_convert(value_type, value_type) override { return false; }
  bool can_convert(std::string, std::shared_ptr<any_node>, value_type) override { return false; }
  std::shared_ptr<binary_function_impl> create_converter(std::string, std::shared_ptr<any_node>, value_type) override { return nullptr; }

  bool has_variable(const std::string &name) override { return variables_.find(name) != variables_.end(); }
  node_type create_variable(const std::string &name, bool) override { return variables_[name]; }
  bool has_function(const std::string &) override { return false; }
  node_type create_function(const std::string &, node_type) override { return factory::create_false(); }
  std::string get_performance_config_key(const std::string, const std::string, const std::string, const std::string, const std::string) const override {
    return "";
  }
};

// ======================================================================
// Helpers
// ======================================================================

static evaluation_context make_context() { return std::make_shared<mock_evaluation_context>(); }
static object_converter make_converter() { return std::make_shared<mock_object_converter>(); }
static object_factory make_factory() { return std::make_shared<mock_object_factory>(); }

static object_factory make_factory_with_variable(const std::string &name, node_type value) {
  auto f = std::make_shared<mock_object_factory>();
  f->variables_[name] = value;
  return f;
}

// ======================================================================
// parser::parse — basic integer expressions
// ======================================================================

TEST(WhereParser, ParseSimpleIntegerLiteral) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "42"));
  EXPECT_TRUE(p.rest.empty());
  EXPECT_EQ(p.result_as_tree(), "42");
}

TEST(WhereParser, ParseNegativeIntegerLiteral) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "-7"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseStringLiteral) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "'hello'"));
  EXPECT_TRUE(p.rest.empty());
  EXPECT_EQ(p.result_as_tree(), "\"hello\"");
}

// ======================================================================
// parser::parse — comparison operators
// ======================================================================

TEST(WhereParser, ParseEqualsExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "1 = 1"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseNotEqualsExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "1 != 2"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseGreaterThanExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "5 > 3"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseLessThanExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "3 < 5"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseGreaterEqualExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "5 >= 5"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseLessEqualExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "5 <= 5"));
  EXPECT_TRUE(p.rest.empty());
}

// ======================================================================
// parser::parse — logical operators
// ======================================================================

TEST(WhereParser, ParseAndExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "1 = 1 and 2 = 2"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseOrExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "1 = 2 or 2 = 2"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseNotExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "not 1 = 2"));
  EXPECT_TRUE(p.rest.empty());
}

// ======================================================================
// parser::parse — string comparisons
// ======================================================================

TEST(WhereParser, ParseStringEquality) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "'foo' = 'foo'"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseStringLikeOperator) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "'foobar' like 'foo'"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseStringNotLikeOperator) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "'foobar' not like 'baz'"));
  EXPECT_TRUE(p.rest.empty());
}

// ======================================================================
// parser::parse — in / not in operators
// ======================================================================

TEST(WhereParser, ParseInExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "2 in (1, 2, 3)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseNotInExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "5 not in (1, 2, 3)"));
  EXPECT_TRUE(p.rest.empty());
}

// ======================================================================
// parser::parse — empty and invalid input
// ======================================================================

TEST(WhereParser, ParseEmptyStringFails) {
  parser p;
  EXPECT_FALSE(p.parse(make_factory(), ""));
}

TEST(WhereParser, ParseGarbageFails) {
  parser p;
  EXPECT_FALSE(p.parse(make_factory(), "!@#$%^&*"));
}

TEST(WhereParser, ParsePartialExpressionFails) {
  parser p;
  // Incomplete expression: "1 =" with nothing on the right
  EXPECT_FALSE(p.parse(make_factory(), "1 ="));
}

// ======================================================================
// parser::parse — parenthesized expressions
// ======================================================================

TEST(WhereParser, ParseParenthesizedExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "(1 = 1)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseNestedParentheses) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "((1 = 1) and (2 = 2))"));
  EXPECT_TRUE(p.rest.empty());
}

// ======================================================================
// parser::parse — variables
// ======================================================================

TEST(WhereParser, ParseVariableExpression) {
  auto f = make_factory_with_variable("status", factory::create_int(0));
  parser p;
  EXPECT_TRUE(p.parse(f, "status = 0"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseUnknownVariableParsesSuccessfully) {
  // The parser itself may still parse unknown variables; validation happens later
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "unknown_var = 1"));
  EXPECT_TRUE(p.rest.empty());
}

// ======================================================================
// parser::parse — function calls (issue #281)
//
// The grammar was extended so that:
//   1. `list_expr` accepts a heterogeneous sequence of identifiers, allowing
//      mixed-type argument lists (variables, string literals, numbers).
//   2. Function calls work both as zero-arg `fn()` and N-arg `fn(a, b, c)`.
// The parser itself does not require functions to be registered — that
// surfaces as a validation error later (via the object_factory). These tests
// only verify the grammar layer accepts the new shapes.
// ======================================================================

TEST(WhereParser, ParseFunctionCallNoArguments) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "fn()"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseFunctionCallSingleIntArgument) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "fn(42)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseFunctionCallSingleStringArgument) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "fn('hello')"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseFunctionCallSingleVariableArgument) {
  auto f = make_factory_with_variable("value", factory::create_int(0));
  parser p;
  EXPECT_TRUE(p.parse(f, "fn(value)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseFunctionCallMixedVariableAndString) {
  // The shape from issue #281: `format_bytes(value, 'MB')`.
  auto f = make_factory_with_variable("value", factory::create_int(0));
  parser p;
  EXPECT_TRUE(p.parse(f, "fn(value, 'MB')"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseFunctionCallMixedVariableAndInt) {
  auto f = make_factory_with_variable("value", factory::create_int(0));
  parser p;
  EXPECT_TRUE(p.parse(f, "fn(value, 1024)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseFunctionCallAllThreeArgumentKinds) {
  // Variable + string literal + int literal — none of these would parse
  // together under the old homogeneous list_expr rules.
  auto f = make_factory_with_variable("value", factory::create_int(0));
  parser p;
  EXPECT_TRUE(p.parse(f, "fn(value, 'tag', 42)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseInWithMixedTypes) {
  // `in (...)` shares the list_expr rule, so it also benefits from
  // heterogeneous arguments.
  auto f = make_factory_with_variable("value", factory::create_int(0));
  parser p;
  EXPECT_TRUE(p.parse(f, "value in (1, 2, value)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseFunctionCallAsComparisonOperand) {
  // Functions must be usable on either side of a comparison so they can drive
  // warning/critical thresholds, e.g. `convert_bytes(value, 'MB') > 100`.
  auto f = make_factory_with_variable("value", factory::create_int(0));
  parser p;
  EXPECT_TRUE(p.parse(f, "fn(value, 'MB') > 100"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseNestedFunctionCalls) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "outer(inner(1), 2)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseMalformedFunctionCallFails) {
  // Unbalanced parens must not parse successfully.
  parser p;
  EXPECT_FALSE(p.parse(make_factory(), "fn(1, 2"));
}

// ======================================================================
// parse_expression — the exported one-shot wrapper used by
// filter_text_renderer for `%(...)` placeholder routing
// ======================================================================

TEST(ParseExpression, SimpleLiteral) {
  const node_type n = parse_expression(make_factory(), "42");
  EXPECT_TRUE(static_cast<bool>(n));
}

TEST(ParseExpression, VariableReference) {
  auto f = make_factory_with_variable("value", factory::create_int(7));
  const node_type n = parse_expression(f, "value");
  EXPECT_TRUE(static_cast<bool>(n));
}

TEST(ParseExpression, FunctionCallWithMixedArguments) {
  auto f = make_factory_with_variable("value", factory::create_int(0));
  const node_type n = parse_expression(f, "fn(value, 'MB')");
  EXPECT_TRUE(static_cast<bool>(n));
}

TEST(ParseExpression, ReturnsNullOnSyntaxError) {
  // Unbalanced parens — parse_expression should return an empty node_type
  // rather than a partial AST so callers can treat null as "no node".
  const node_type n = parse_expression(make_factory(), "fn(1, 2");
  EXPECT_FALSE(static_cast<bool>(n));
}

TEST(ParseExpression, ReturnsNullOnTrailingGarbage) {
  // Parser must reject input it can't fully consume so callers don't see a
  // valid-looking node from an only-partially-parsed expression.
  const node_type n = parse_expression(make_factory(), "1 = 1 garbage");
  EXPECT_FALSE(static_cast<bool>(n));
}

TEST(ParseExpression, ReturnsNullOnEmptyInput) {
  const node_type n = parse_expression(make_factory(), "");
  EXPECT_FALSE(static_cast<bool>(n));
}

// ======================================================================
// Invalid function-call shapes — the grammar must reject malformed lists
// even though the placeholder parser captures them as bodies.
//
// Each test here uses parse_expression (which checks rest.empty()) so the
// pass/fail signal is unambiguous: a clean parse returns a node, anything
// the grammar can't consume in full returns an empty node.
// ======================================================================

TEST(ParseExpression, FunctionCallWithBareCommaFails) { EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "fn(,)"))); }

TEST(ParseExpression, FunctionCallWithTrailingCommaFails) { EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "fn(a,)"))); }

TEST(ParseExpression, FunctionCallWithLeadingCommaFails) { EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "fn(,a)"))); }

TEST(ParseExpression, FunctionCallWithDoubleCommaFails) { EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "fn(a,,b)"))); }

TEST(ParseExpression, FunctionCallMissingCommaSeparatorFails) {
  // `a b` is not a valid list element — there's no separator between them.
  EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "fn(a b)")));
}

TEST(ParseExpression, FunctionCallMissingClosingParenFails) { EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "fn(a, b"))); }

TEST(ParseExpression, FunctionCallMissingOpeningParenFails) {
  // `fn a)` — no '(' after the name. The parser will see `fn` as a variable
  // and leave the rest unconsumed.
  EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "fn a)")));
}

TEST(ParseExpression, FunctionCallUnbalancedNestedFails) {
  // Nested open without a matching close inside the arg list.
  EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "fn(g(a, b)")));
}

TEST(ParseExpression, InListEmptyFails) {
  // `in ()` — empty list; identifier % ',' requires at least one element.
  EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "x in ()")));
}

TEST(ParseExpression, InListTrailingCommaFails) { EXPECT_FALSE(static_cast<bool>(parse_expression(make_factory(), "x in (1, 2,)"))); }

// ======================================================================
// Valid edge cases worth pinning down
// ======================================================================

TEST(ParseExpression, FunctionCallManyArguments) {
  // Stress the heterogeneous arg list with a longer mix.
  auto f = make_factory_with_variable("v", factory::create_int(0));
  const node_type n = parse_expression(f, "fn(1, 'a', 2.5, v, 'b', 99)");
  EXPECT_TRUE(static_cast<bool>(n));
}

TEST(ParseExpression, NestedFunctionCallEvaluates) {
  // Inner call as one of the args of the outer call.
  auto f = make_factory_with_variable("v", factory::create_int(0));
  const node_type n = parse_expression(f, "outer(inner(v), 'tag')");
  EXPECT_TRUE(static_cast<bool>(n));
}

TEST(ParseExpression, FunctionCallSingleElementList) {
  // `in (x)` is the degenerate one-element list — same grammar rule as
  // function calls.
  auto f = make_factory_with_variable("v", factory::create_int(0));
  const node_type n = parse_expression(f, "v in (1)");
  EXPECT_TRUE(static_cast<bool>(n));
}

TEST(ParseExpression, FunctionCallOnBothSidesOfComparison) {
  // Both operands of an `=` can be function calls.
  auto f = make_factory_with_variable("v", factory::create_int(0));
  const node_type n = parse_expression(f, "fn(v) = fn('default')");
  EXPECT_TRUE(static_cast<bool>(n));
}

TEST(ParseExpression, NegatedFunctionCall) {
  auto f = make_factory_with_variable("v", factory::create_int(0));
  const node_type n = parse_expression(f, "not fn(v)");
  EXPECT_TRUE(static_cast<bool>(n));
}

TEST(ParseExpression, FunctionCallCombinedWithAndOr) {
  auto f = make_factory_with_variable("v", factory::create_int(0));
  const node_type n = parse_expression(f, "fn(v) > 0 and other(v, 'tag') = 1");
  EXPECT_TRUE(static_cast<bool>(n));
}

// ======================================================================
// parser::derive_types
// ======================================================================

TEST(WhereParser, DeriveTypesSimpleInt) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "42"));
  auto converter = make_converter();
  EXPECT_TRUE(p.derive_types(converter));
  EXPECT_FALSE(converter->has_error());
}

TEST(WhereParser, DeriveTypesComparison) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "1 = 1"));
  auto converter = make_converter();
  EXPECT_TRUE(p.derive_types(converter));
  EXPECT_FALSE(converter->has_error());
}

// ======================================================================
// parser::static_eval
// ======================================================================

TEST(WhereParser, StaticEvalSimpleExpression) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "1 = 1"));
  auto ctx = make_context();
  EXPECT_TRUE(p.static_eval(ctx));
  EXPECT_FALSE(ctx->has_error());
}

TEST(WhereParser, StaticEvalDoesNotProduceErrors) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "5 > 3"));
  auto ctx = make_context();
  EXPECT_TRUE(p.static_eval(ctx));
  EXPECT_FALSE(ctx->has_error());
}

// ======================================================================
// parser::bind
// ======================================================================

TEST(WhereParser, BindSimpleExpression) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "1 = 1"));
  auto converter = make_converter();
  EXPECT_TRUE(p.bind(converter));
  EXPECT_FALSE(converter->has_error());
}

TEST(WhereParser, BindStringExpression) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "'hello' = 'hello'"));
  auto converter = make_converter();
  EXPECT_TRUE(p.bind(converter));
  EXPECT_FALSE(converter->has_error());
}

// ======================================================================
// parser::evaluate — integer comparisons
// ======================================================================

TEST(WhereParser, EvaluateEqualsTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 = 1"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, EvaluateEqualsFalse) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 = 2"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_FALSE(result.is_true());
}

TEST(WhereParser, EvaluateGreaterThanTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "5 > 3"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, EvaluateGreaterThanFalse) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "3 > 5"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_FALSE(result.is_true());
}

TEST(WhereParser, EvaluateLessThanTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "3 < 5"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, EvaluateNotEqualsTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 != 2"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, EvaluateNotEqualsFalse) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 != 1"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_FALSE(result.is_true());
}

TEST(WhereParser, EvaluateGreaterEqualTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "5 >= 5"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, EvaluateLessEqualTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "5 <= 5"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// parser::evaluate — logical operators
// ======================================================================

TEST(WhereParser, EvaluateAndBothTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 = 1 and 2 = 2"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, EvaluateAndOneFalse) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 = 1 and 2 = 3"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_FALSE(result.is_true());
}

TEST(WhereParser, EvaluateOrOneTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 = 2 or 2 = 2"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, EvaluateOrBothFalse) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 = 2 or 3 = 4"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// parser::evaluate — string comparisons
// ======================================================================

TEST(WhereParser, EvaluateStringEqualsTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "'hello' = 'hello'"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, EvaluateStringEqualsFalse) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "'hello' = 'world'"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_FALSE(result.is_true());
}

TEST(WhereParser, EvaluateStringNotEqualsTrue) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "'hello' != 'world'"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// parser::result_as_tree
// ======================================================================

TEST(WhereParser, ResultAsTreeForLiteral) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "42"));
  EXPECT_FALSE(p.result_as_tree().empty());
}

TEST(WhereParser, ResultAsTreeForComparison) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "1 = 2"));
  std::string tree = p.result_as_tree();
  EXPECT_FALSE(tree.empty());
  // The tree representation should contain both operands
  EXPECT_NE(tree.find("1"), std::string::npos);
  EXPECT_NE(tree.find("2"), std::string::npos);
}

TEST(WhereParser, ResultAsTreeWithContext) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "1 = 1"));
  auto ctx = make_context();
  std::string tree = p.result_as_tree(ctx);
  EXPECT_FALSE(tree.empty());
}

// ======================================================================
// parser::collect_perfkeys
// ======================================================================

TEST(WhereParser, CollectPerfkeysSimpleExpression) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "1 = 1"));
  auto ctx = make_context();
  performance_collector collector;
  EXPECT_TRUE(p.collect_perfkeys(ctx, collector));
  EXPECT_FALSE(ctx->has_error());
}

// ======================================================================
// parser::require_object
// ======================================================================

TEST(WhereParser, RequireObjectLiteralExpression) {
  parser p;
  ASSERT_TRUE(p.parse(make_factory(), "1 = 1"));
  auto ctx = make_context();
  // A literal-only expression should not require an object
  EXPECT_FALSE(p.require_object(ctx));
}

TEST(WhereParser, RequireObjectVariableExpression) {
  auto f = make_factory_with_variable("status", factory::create_int(0));
  parser p;
  ASSERT_TRUE(p.parse(f, "status = 0"));
  const auto ctx = make_context();
  // An expression referencing a variable typically requires an object
  // TODO: Fix this test
  // EXPECT_TRUE(p.require_object(ctx));
}

// ======================================================================
// parser::parse — complex expressions
// ======================================================================

TEST(WhereParser, ParseComplexAndOrExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "(1 = 1 or 2 = 3) and (4 > 2)"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, ParseMultipleAndsExpression) {
  parser p;
  EXPECT_TRUE(p.parse(make_factory(), "1 = 1 and 2 = 2 and 3 = 3"));
  EXPECT_TRUE(p.rest.empty());
}

TEST(WhereParser, EvaluateComplexAndOrExpression) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "(1 = 1 or 2 = 3) and (4 > 2)"));
  ASSERT_TRUE(p.derive_types(std::dynamic_pointer_cast<object_converter_interface>(f)));
  auto ctx = make_context();
  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// parser — full pipeline: parse → derive_types → bind → static_eval → evaluate
// ======================================================================

TEST(WhereParser, FullPipelineIntComparison) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "10 > 5"));

  auto converter = std::dynamic_pointer_cast<object_converter_interface>(f);
  ASSERT_TRUE(p.derive_types(converter));
  ASSERT_TRUE(p.bind(converter));

  auto ctx = make_context();
  ASSERT_TRUE(p.static_eval(ctx));

  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, FullPipelineStringComparison) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "'abc' = 'abc'"));

  auto converter = std::dynamic_pointer_cast<object_converter_interface>(f);
  ASSERT_TRUE(p.derive_types(converter));
  ASSERT_TRUE(p.bind(converter));

  auto ctx = make_context();
  ASSERT_TRUE(p.static_eval(ctx));

  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}

TEST(WhereParser, FullPipelineLogicalAnd) {
  parser p;
  auto f = make_factory();
  ASSERT_TRUE(p.parse(f, "1 = 1 and 'x' = 'x'"));

  auto converter = std::dynamic_pointer_cast<object_converter_interface>(f);
  ASSERT_TRUE(p.derive_types(converter));
  ASSERT_TRUE(p.bind(converter));

  auto ctx = make_context();
  ASSERT_TRUE(p.static_eval(ctx));

  value_container result = p.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(result.is_true());
}
