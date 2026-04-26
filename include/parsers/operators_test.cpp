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

#include <list>
#include <parsers/operators.hpp>
#include <parsers/where/helpers.hpp>
#include <parsers/where/list_node.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/value_node.hpp>
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
  void error(std::string msg) override { error_ += msg; }
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
  void error(std::string msg) override { error_ += msg; }
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
// Helpers
// ======================================================================

static evaluation_context make_context() { return std::make_shared<mock_evaluation_context>(); }
static object_converter make_converter() { return std::make_shared<mock_object_converter>(); }

static node_type make_int(long long v) { return factory::create_int(v); }
static node_type make_float(double v) { return factory::create_float(v); }
static node_type make_string(const std::string &v) { return factory::create_string(v); }

static node_type make_int_list(std::initializer_list<long long> values) { return factory::create_list(std::list<long long>(values)); }

static node_type make_string_list(std::initializer_list<std::string> values) { return factory::create_list(std::list<std::string>(values)); }

static node_type make_float_list(std::initializer_list<double> values) { return factory::create_list(std::list<double>(values)); }

// Helper to evaluate a binary operator directly
static value_container eval_bin_op(operators op, node_type lhs, node_type rhs, evaluation_context ctx) {
  auto bin_op = op_factory::get_binary_operator(op, lhs, rhs);
  node_type result = bin_op->evaluate(ctx, lhs, rhs);
  return result->get_value(ctx, type_int);
}

// ======================================================================
// op_factory::get_binary_operator — returns non-null for all valid ops
// ======================================================================

TEST(OpFactory, GetBinaryOperatorReturnsNonNullForAllOps) {
  auto lhs = make_int(1);
  auto rhs = make_int(2);
  EXPECT_NE(op_factory::get_binary_operator(op_eq, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_ne, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_gt, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_lt, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_ge, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_le, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_and, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_or, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_like, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_not_like, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_regexp, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_not_regexp, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_in, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_nin, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_binand, lhs, rhs), nullptr);
  EXPECT_NE(op_factory::get_binary_operator(op_binor, lhs, rhs), nullptr);
}

// ======================================================================
// op_factory::is_binary_function
// ======================================================================

TEST(OpFactory, IsBinaryFunctionConvert) { EXPECT_TRUE(op_factory::is_binary_function("convert")); }

TEST(OpFactory, IsBinaryFunctionAutoConvert) { EXPECT_TRUE(op_factory::is_binary_function("auto_convert")); }

TEST(OpFactory, IsBinaryFunctionNeg) { EXPECT_TRUE(op_factory::is_binary_function("neg")); }

TEST(OpFactory, IsBinaryFunctionUnknown) {
  EXPECT_FALSE(op_factory::is_binary_function("unknown_function"));
  EXPECT_FALSE(op_factory::is_binary_function(""));
}

// ======================================================================
// op_factory::get_binary_function
// ======================================================================

TEST(OpFactory, GetBinaryFunctionConvertReturnsNonNull) {
  auto ctx = make_context();
  auto subject = make_int(42);
  auto fun = op_factory::get_binary_function(ctx, "convert", subject);
  EXPECT_NE(fun, nullptr);
}

TEST(OpFactory, GetBinaryFunctionNegReturnsNonNull) {
  auto ctx = make_context();
  auto subject = make_int(42);
  auto fun = op_factory::get_binary_function(ctx, "neg", subject);
  EXPECT_NE(fun, nullptr);
}

// ======================================================================
// op_factory::get_unary_operator
// ======================================================================

TEST(OpFactory, GetUnaryOperatorNotReturnsNonNull) {
  auto un_op = op_factory::get_unary_operator(op_not);
  EXPECT_NE(un_op, nullptr);
}

// ======================================================================
// operator_eq — integer
// ======================================================================

TEST(OperatorEq, IntEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_int(42), make_int(42), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorEq, IntNotEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_int(42), make_int(99), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorEq, IntZeroEqualsZero) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_int(0), make_int(0), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorEq, IntNegativeEquals) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_int(-5), make_int(-5), ctx);
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// operator_eq — float
// ======================================================================

TEST(OperatorEq, FloatEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_float(3.14), make_float(3.14), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorEq, FloatNotEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_float(3.14), make_float(2.71), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_eq — string
// ======================================================================

TEST(OperatorEq, StringEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_string("hello"), make_string("hello"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorEq, StringNotEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_string("hello"), make_string("world"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorEq, StringEmptyEquals) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_string(""), make_string(""), ctx);
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// operator_ne — integer
// ======================================================================

TEST(OperatorNe, IntNotEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ne, make_int(1), make_int(2), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNe, IntEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ne, make_int(5), make_int(5), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_ne — float
// ======================================================================

TEST(OperatorNe, FloatNotEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ne, make_float(1.0), make_float(2.0), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNe, FloatEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ne, make_float(3.0), make_float(3.0), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_ne — string
// ======================================================================

TEST(OperatorNe, StringNotEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ne, make_string("abc"), make_string("def"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNe, StringEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ne, make_string("abc"), make_string("abc"), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_gt — integer
// ======================================================================

TEST(OperatorGt, IntGreaterReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_int(10), make_int(5), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGt, IntEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_int(5), make_int(5), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorGt, IntLessReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_int(3), make_int(7), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_gt — float
// ======================================================================

TEST(OperatorGt, FloatGreaterReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_float(3.14), make_float(2.71), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGt, FloatEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_float(2.5), make_float(2.5), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_gt — string
// ======================================================================

TEST(OperatorGt, StringGreaterReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_string("b"), make_string("a"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGt, StringLessReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_string("a"), make_string("b"), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_lt — integer
// ======================================================================

TEST(OperatorLt, IntLessReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_int(3), make_int(7), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLt, IntEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_int(5), make_int(5), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorLt, IntGreaterReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_int(7), make_int(3), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_lt — float
// ======================================================================

TEST(OperatorLt, FloatLessReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_float(1.5), make_float(2.5), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLt, FloatEqualReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_float(2.5), make_float(2.5), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_lt — string
// ======================================================================

TEST(OperatorLt, StringLessReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_string("a"), make_string("b"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLt, StringGreaterReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_string("b"), make_string("a"), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_le — integer
// ======================================================================

TEST(OperatorLe, IntLessReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_int(3), make_int(5), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLe, IntEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_int(5), make_int(5), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLe, IntGreaterReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_int(7), make_int(5), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_le — float
// ======================================================================

TEST(OperatorLe, FloatLessReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_float(1.0), make_float(2.0), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLe, FloatEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_float(2.0), make_float(2.0), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLe, FloatGreaterReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_float(3.0), make_float(2.0), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_le — string
// ======================================================================

TEST(OperatorLe, StringLessReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_string("a"), make_string("b"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLe, StringEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_string("a"), make_string("a"), ctx);
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// operator_ge — integer
// ======================================================================

TEST(OperatorGe, IntGreaterReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_int(7), make_int(5), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGe, IntEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_int(5), make_int(5), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGe, IntLessReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_int(3), make_int(5), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_ge — float
// ======================================================================

TEST(OperatorGe, FloatGreaterReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_float(3.0), make_float(2.0), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGe, FloatEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_float(2.0), make_float(2.0), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGe, FloatLessReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_float(1.0), make_float(2.0), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_ge — string
// ======================================================================

TEST(OperatorGe, StringGreaterReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_string("b"), make_string("a"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGe, StringEqualReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_string("a"), make_string("a"), ctx);
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// operator_and — integer
// ======================================================================

TEST(OperatorAnd, TrueTrueReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_and, make_int(1), make_int(1), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorAnd, TrueFalseReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_and, make_int(1), make_int(0), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorAnd, FalseTrueReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_and, make_int(0), make_int(1), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorAnd, FalseFalseReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_and, make_int(0), make_int(0), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorAnd, ShortCircuitOnFalseLeft) {
  auto ctx = make_context();
  // When left is false (0) and not unsure, should short-circuit
  auto result = eval_bin_op(op_and, make_int(0), make_int(1), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_or — integer
// ======================================================================

TEST(OperatorOr, TrueTrueReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_or, make_int(1), make_int(1), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorOr, TrueFalseReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_or, make_int(1), make_int(0), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorOr, FalseTrueReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_or, make_int(0), make_int(1), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorOr, FalseFalseReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_or, make_int(0), make_int(0), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorOr, ShortCircuitOnTrueLeft) {
  auto ctx = make_context();
  // When left is true (1) and not unsure, should short-circuit
  auto result = eval_bin_op(op_or, make_int(1), make_int(0), ctx);
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// operator_like — string matching
// ======================================================================

TEST(OperatorLike, SubstringMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_string("hello world"), make_string("world"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLike, NoMatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_string("hello world"), make_string("xyz"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorLike, CaseInsensitiveMatch) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_string("Hello World"), make_string("hello"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLike, BothEmptyReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_string(""), make_string(""), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLike, LeftEmptyRightNotReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_string(""), make_string("abc"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorLike, LeftNotEmptyRightEmptyReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_string("hello"), make_string(""), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorLike, SmallerSearchInLarger) {
  // When s1.size() <= s2.size(), it checks s2.find(s1)
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_string("he"), make_string("hello"), ctx);
  EXPECT_TRUE(result.is_true());
}

// like on int — pattern is matched against the textual representation of the int,
// so e.g. `cpu like "8"` matches 8, 80, 800, 18, ...
TEST(OperatorLike, IntExactDigitMatches) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_int(80), make_string("80"), ctx);
  EXPECT_TRUE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorLike, IntSubstringDigitMatches) {
  auto ctx = make_context();
  // "8" is a substring of "180"
  auto result = eval_bin_op(op_like, make_int(180), make_string("8"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLike, IntNoMatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_int(42), make_string("9"), ctx);
  EXPECT_FALSE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorLike, FloatSubstringMatches) {
  auto ctx = make_context();
  // 3.14 -> "3.14" contains "14"
  auto result = eval_bin_op(op_like, make_float(3.14), make_string("14"), ctx);
  EXPECT_TRUE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorLike, FloatNoMatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_like, make_float(3.14), make_string("99"), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_not_like — string matching
// ======================================================================

TEST(OperatorNotLike, SubstringMatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_string("hello world"), make_string("world"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorNotLike, NoMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_string("hello world"), make_string("xyz"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNotLike, BothEmptyReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_string(""), make_string(""), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorNotLike, LeftEmptyRightNotReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_string(""), make_string("abc"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNotLike, CaseInsensitiveNoMatch) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_string("Hello World"), make_string("xyz"), ctx);
  EXPECT_TRUE(result.is_true());
}

// not_like on int / float — pattern is matched against the textual representation
TEST(OperatorNotLike, IntMatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_int(180), make_string("8"), ctx);
  EXPECT_FALSE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorNotLike, IntNoMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_int(42), make_string("9"), ctx);
  EXPECT_TRUE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorNotLike, FloatMatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_float(3.14), make_string("14"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorNotLike, FloatNoMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_like, make_float(3.14), make_string("99"), ctx);
  EXPECT_TRUE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

// ======================================================================
// operator_regexp — regex matching
// ======================================================================

TEST(OperatorRegexp, SimpleMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_regexp, make_string("hello"), make_string("hello"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorRegexp, RegexPatternMatch) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_regexp, make_string("hello123"), make_string("hello\\d+"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorRegexp, RegexPatternNoMatch) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_regexp, make_string("hello"), make_string("world"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorRegexp, RegexWildcard) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_regexp, make_string("abc123"), make_string(".*123"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorRegexp, InvalidRegexReturnsError) {
  auto ctx = make_context();
  auto bin_op = op_factory::get_binary_operator(op_regexp, make_string("test"), make_string("[invalid"));
  auto result = bin_op->evaluate(ctx, make_string("test"), make_string("[invalid"));
  EXPECT_TRUE(ctx->has_error());
}

// regexp on int / float — pattern is matched against the textual representation
TEST(OperatorRegexp, IntExactMatch) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_regexp, make_int(80), make_string("80"), ctx);
  EXPECT_TRUE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorRegexp, IntPatternMatch) {
  auto ctx = make_context();
  // 180 -> "180" matches "1.*"
  auto result = eval_bin_op(op_regexp, make_int(180), make_string("1.*"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorRegexp, IntPatternNoMatch) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_regexp, make_int(42), make_string("9.*"), ctx);
  EXPECT_FALSE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorRegexp, FloatPatternMatch) {
  auto ctx = make_context();
  // 3.14 -> "3.14" matches "3\..*"
  auto result = eval_bin_op(op_regexp, make_float(3.14), make_string("3\\..*"), ctx);
  EXPECT_TRUE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorRegexp, FloatPatternNoMatch) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_regexp, make_float(3.14), make_string("9.*"), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_not_regexp — negated regex matching
// ======================================================================

TEST(OperatorNotRegexp, MatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_regexp, make_string("hello"), make_string("hello"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorNotRegexp, NoMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_regexp, make_string("hello"), make_string("world"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNotRegexp, RegexPatternNoMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_regexp, make_string("hello"), make_string("\\d+"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNotRegexp, InvalidRegexReturnsError) {
  auto ctx = make_context();
  auto bin_op = op_factory::get_binary_operator(op_not_regexp, make_string("test"), make_string("[invalid"));
  auto result = bin_op->evaluate(ctx, make_string("test"), make_string("[invalid"));
  EXPECT_TRUE(ctx->has_error());
}

// not_regexp on int / float — pattern is matched against the textual representation
TEST(OperatorNotRegexp, IntMatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_regexp, make_int(80), make_string("80"), ctx);
  EXPECT_FALSE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorNotRegexp, IntNoMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_regexp, make_int(42), make_string("9.*"), ctx);
  EXPECT_TRUE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

TEST(OperatorNotRegexp, FloatMatchReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_regexp, make_float(3.14), make_string("3\\..*"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorNotRegexp, FloatNoMatchReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_regexp, make_float(3.14), make_string("9.*"), ctx);
  EXPECT_TRUE(result.is_true());
  EXPECT_FALSE(ctx->has_error());
}

// ======================================================================
// operator_in — integer list
// ======================================================================

TEST(OperatorIn, IntFoundInListReturnsTrue) {
  auto ctx = make_context();
  auto lhs = make_int(2);
  auto rhs = make_int_list({1, 2, 3});
  auto bin_op = op_factory::get_binary_operator(op_in, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_TRUE(result->get_value(ctx, type_int).is_true());
}

TEST(OperatorIn, IntNotFoundInListReturnsFalse) {
  auto ctx = make_context();
  auto lhs = make_int(5);
  auto rhs = make_int_list({1, 2, 3});
  auto bin_op = op_factory::get_binary_operator(op_in, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_FALSE(result->get_value(ctx, type_int).is_true());
}

// ======================================================================
// operator_in — string list
// ======================================================================

TEST(OperatorIn, StringFoundInListReturnsTrue) {
  auto ctx = make_context();
  auto lhs = make_string("hello");
  auto rhs = make_string_list({"hello", "world"});
  auto bin_op = op_factory::get_binary_operator(op_in, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_TRUE(result->get_value(ctx, type_int).is_true());
}

TEST(OperatorIn, StringNotFoundInListReturnsFalse) {
  auto ctx = make_context();
  auto lhs = make_string("xyz");
  auto rhs = make_string_list({"hello", "world"});
  auto bin_op = op_factory::get_binary_operator(op_in, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_FALSE(result->get_value(ctx, type_int).is_true());
}

// ======================================================================
// operator_in — float list
// ======================================================================

TEST(OperatorIn, FloatFoundInListReturnsTrue) {
  auto ctx = make_context();
  auto lhs = make_float(2.5);
  auto rhs = make_float_list({1.0, 2.5, 3.0});
  auto bin_op = op_factory::get_binary_operator(op_in, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_TRUE(result->get_value(ctx, type_int).is_true());
}

TEST(OperatorIn, FloatNotFoundInListReturnsFalse) {
  auto ctx = make_context();
  auto lhs = make_float(4.0);
  auto rhs = make_float_list({1.0, 2.5, 3.0});
  auto bin_op = op_factory::get_binary_operator(op_in, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_FALSE(result->get_value(ctx, type_int).is_true());
}

// ======================================================================
// operator_not_in — integer list
// ======================================================================

TEST(OperatorNotIn, IntNotInListReturnsTrue) {
  auto ctx = make_context();
  auto lhs = make_int(5);
  auto rhs = make_int_list({1, 2, 3});
  auto bin_op = op_factory::get_binary_operator(op_nin, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_TRUE(result->get_value(ctx, type_int).is_true());
}

TEST(OperatorNotIn, IntInListReturnsFalse) {
  auto ctx = make_context();
  auto lhs = make_int(2);
  auto rhs = make_int_list({1, 2, 3});
  auto bin_op = op_factory::get_binary_operator(op_nin, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_FALSE(result->get_value(ctx, type_int).is_true());
}

// ======================================================================
// operator_not_in — string list
// ======================================================================

TEST(OperatorNotIn, StringNotInListReturnsTrue) {
  auto ctx = make_context();
  auto lhs = make_string("xyz");
  auto rhs = make_string_list({"hello", "world"});
  auto bin_op = op_factory::get_binary_operator(op_nin, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_TRUE(result->get_value(ctx, type_int).is_true());
}

TEST(OperatorNotIn, StringInListReturnsFalse) {
  auto ctx = make_context();
  auto lhs = make_string("hello");
  auto rhs = make_string_list({"hello", "world"});
  auto bin_op = op_factory::get_binary_operator(op_nin, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_FALSE(result->get_value(ctx, type_int).is_true());
}

// ======================================================================
// operator_not_in — float list
// ======================================================================

TEST(OperatorNotIn, FloatNotInListReturnsTrue) {
  auto ctx = make_context();
  auto lhs = make_float(4.0);
  auto rhs = make_float_list({1.0, 2.5, 3.0});
  auto bin_op = op_factory::get_binary_operator(op_nin, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_TRUE(result->get_value(ctx, type_int).is_true());
}

TEST(OperatorNotIn, FloatInListReturnsFalse) {
  auto ctx = make_context();
  auto lhs = make_float(2.5);
  auto rhs = make_float_list({1.0, 2.5, 3.0});
  auto bin_op = op_factory::get_binary_operator(op_nin, lhs, rhs);
  auto result = bin_op->evaluate(ctx, lhs, rhs);
  EXPECT_FALSE(result->get_value(ctx, type_int).is_true());
}

// ======================================================================
// operator_not (unary) — via op_factory
// ======================================================================

TEST(OperatorNot, NotOnBoolTrueReturnsFalse) {
  auto ctx = make_context();
  auto subject = make_int(1);
  subject->set_type(type_bool);
  auto un_op = op_factory::get_unary_operator(op_not);
  auto result = un_op->evaluate(ctx, subject);
  EXPECT_EQ(result->get_int_value(ctx), 0);
}

TEST(OperatorNot, NotOnBoolFalseReturnsTrue) {
  auto ctx = make_context();
  auto subject = make_int(0);
  subject->set_type(type_bool);
  auto un_op = op_factory::get_unary_operator(op_not);
  auto result = un_op->evaluate(ctx, subject);
  EXPECT_EQ(result->get_int_value(ctx), 1);
}

TEST(OperatorNot, NotOnIntNegates) {
  auto ctx = make_context();
  auto subject = make_int(42);
  auto un_op = op_factory::get_unary_operator(op_not);
  auto result = un_op->evaluate(ctx, subject);
  EXPECT_EQ(result->get_int_value(ctx), -42);
}

TEST(OperatorNot, NotOnNegativeInt) {
  auto ctx = make_context();
  auto subject = make_int(-10);
  auto un_op = op_factory::get_unary_operator(op_not);
  auto result = un_op->evaluate(ctx, subject);
  EXPECT_EQ(result->get_int_value(ctx), 10);
}

TEST(OperatorNot, NotOnZero) {
  auto ctx = make_context();
  auto subject = make_int(0);
  auto un_op = op_factory::get_unary_operator(op_not);
  auto result = un_op->evaluate(ctx, subject);
  EXPECT_EQ(result->get_int_value(ctx), 0);
}

// ======================================================================
// function_convert — via op_factory::get_binary_function
// ======================================================================

TEST(FunctionConvert, ConvertFloatToIntRounds) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_float(3.7));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_int, ctx, list);
  EXPECT_EQ(result->get_int_value(ctx), 4);
}

TEST(FunctionConvert, ConvertFloatToIntRoundsDown) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_float(3.2));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_int, ctx, list);
  EXPECT_EQ(result->get_int_value(ctx), 3);
}

TEST(FunctionConvert, ConvertIntToFloat) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(42));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_float, ctx, list);
  EXPECT_DOUBLE_EQ(result->get_float_value(ctx), 42.0);
}

TEST(FunctionConvert, ConvertNoArgumentsReturnsError) {
  auto ctx = make_context();
  auto list = factory::create_list();
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_int, ctx, list);
  EXPECT_TRUE(ctx->has_error());
}

TEST(FunctionConvert, ConvertTimeSSeconds) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(60));
  list->push_back(make_string("s"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_date, ctx, list);
  // The result should be now + 60 seconds
  EXPECT_GT(result->get_int_value(ctx), 0);
}

TEST(FunctionConvert, ConvertTimeMMinutes) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(5));
  list->push_back(make_string("m"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_date, ctx, list);
  EXPECT_GT(result->get_int_value(ctx), 0);
}

TEST(FunctionConvert, ConvertTimeHHours) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(2));
  list->push_back(make_string("h"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_date, ctx, list);
  EXPECT_GT(result->get_int_value(ctx), 0);
}

TEST(FunctionConvert, ConvertTimeDDays) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(1));
  list->push_back(make_string("d"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_date, ctx, list);
  EXPECT_GT(result->get_int_value(ctx), 0);
}

TEST(FunctionConvert, ConvertTimeWWeeks) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(1));
  list->push_back(make_string("w"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_date, ctx, list);
  EXPECT_GT(result->get_int_value(ctx), 0);
}

TEST(FunctionConvert, ConvertSizeBytes) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(100));
  list->push_back(make_string("b"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_size, ctx, list);
  EXPECT_EQ(result->get_int_value(ctx), 100);
}

TEST(FunctionConvert, ConvertSizeKilobytes) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(1));
  list->push_back(make_string("k"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_size, ctx, list);
  EXPECT_EQ(result->get_int_value(ctx), 1024);
}

TEST(FunctionConvert, ConvertSizeMegabytes) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(1));
  list->push_back(make_string("M"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_size, ctx, list);
  EXPECT_EQ(result->get_int_value(ctx), 1024 * 1024);
}

TEST(FunctionConvert, ConvertSizeGigabytes) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(1));
  list->push_back(make_string("G"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_size, ctx, list);
  EXPECT_EQ(result->get_int_value(ctx), 1024LL * 1024 * 1024);
}

TEST(FunctionConvert, ConvertSizeTerabytes) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(1));
  list->push_back(make_string("T"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_size, ctx, list);
  EXPECT_EQ(result->get_int_value(ctx), 1024LL * 1024 * 1024 * 1024);
}

TEST(FunctionConvert, ConvertUnknownTypeWithUnitReturnsError) {
  auto ctx = make_context();
  auto list = factory::create_list();
  list->push_back(make_int(100));
  list->push_back(make_string("x"));
  auto fun = op_factory::get_binary_function(ctx, "convert", list);
  auto result = fun->evaluate(type_string, ctx, list);
  EXPECT_TRUE(ctx->has_error());
}

// ======================================================================
// neg function — via op_factory::get_binary_function
// ======================================================================

TEST(FunctionNeg, NegOnBoolTrueReturnsFalse) {
  auto ctx = make_context();
  auto subject = make_int(1);
  subject->set_type(type_bool);
  auto fun = op_factory::get_binary_function(ctx, "neg", subject);
  auto result = fun->evaluate(type_bool, ctx, subject);
  EXPECT_EQ(result->get_int_value(ctx), 0);
}

TEST(FunctionNeg, NegOnBoolFalseReturnsTrue) {
  auto ctx = make_context();
  auto subject = make_int(0);
  subject->set_type(type_bool);
  auto fun = op_factory::get_binary_function(ctx, "neg", subject);
  auto result = fun->evaluate(type_bool, ctx, subject);
  EXPECT_EQ(result->get_int_value(ctx), 1);
}

TEST(FunctionNeg, NegOnIntNegates) {
  auto ctx = make_context();
  auto subject = make_int(42);
  auto fun = op_factory::get_binary_function(ctx, "neg", subject);
  auto result = fun->evaluate(type_int, ctx, subject);
  EXPECT_EQ(result->get_int_value(ctx), -42);
}

// ======================================================================
// operator_binand / operator_binor — these delegate to and/or
// ======================================================================

TEST(OperatorBinAnd, TrueTrueReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_binand, make_int(1), make_int(1), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorBinAnd, TrueFalseReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_binand, make_int(1), make_int(0), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorBinOr, FalseTrueReturnsTrue) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_binor, make_int(0), make_int(1), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorBinOr, FalseFalseReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_binor, make_int(0), make_int(0), ctx);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// operator_false — fallback for unknown operators
// ======================================================================

TEST(OperatorFalse, UnknownOperatorReturnsError) {
  auto ctx = make_context();
  // op_inv is not handled, so it falls through to operator_false
  auto bin_op = op_factory::get_binary_operator(op_inv, make_int(1), make_int(2));
  auto result = bin_op->evaluate(ctx, make_int(1), make_int(2));
  EXPECT_TRUE(ctx->has_error());
}

// ======================================================================
// Edge cases with negative and large numbers
// ======================================================================

TEST(OperatorEq, LargeNumbers) {
  auto ctx = make_context();
  long long large = 9999999999LL;
  auto result = eval_bin_op(op_eq, make_int(large), make_int(large), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGt, NegativeNumbers) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_int(-1), make_int(-5), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLt, NegativeNumbers) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_int(-10), make_int(-5), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorGe, BoundaryEqualValues) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_ge, make_int(0), make_int(0), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLe, BoundaryEqualValues) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_le, make_int(0), make_int(0), ctx);
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// String comparison edge cases
// ======================================================================

TEST(OperatorEq, StringCaseSensitive) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_eq, make_string("Hello"), make_string("hello"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorGt, StringLexicographic) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_gt, make_string("z"), make_string("a"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorLt, StringLexicographic) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_lt, make_string("a"), make_string("z"), ctx);
  EXPECT_TRUE(result.is_true());
}

// ======================================================================
// Regexp edge cases
// ======================================================================

TEST(OperatorRegexp, FullMatch) {
  auto ctx = make_context();
  // regex_match requires full match
  auto result = eval_bin_op(op_regexp, make_string("abc"), make_string("abc"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorRegexp, PartialDoesNotMatch) {
  auto ctx = make_context();
  // regex_match requires full match, so "bc" should not match "abc"
  auto result = eval_bin_op(op_regexp, make_string("abc"), make_string("bc"), ctx);
  EXPECT_FALSE(result.is_true());
}

TEST(OperatorRegexp, DotStarMatchesAnything) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_regexp, make_string("anything goes here"), make_string(".*"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNotRegexp, PartialDoesNotMatch) {
  auto ctx = make_context();
  // regex_match requires full match, "bc" does not match "abc", so not_regexp returns true
  auto result = eval_bin_op(op_not_regexp, make_string("abc"), make_string("bc"), ctx);
  EXPECT_TRUE(result.is_true());
}

TEST(OperatorNotRegexp, DotStarMatchesReturnsFalse) {
  auto ctx = make_context();
  auto result = eval_bin_op(op_not_regexp, make_string("anything"), make_string(".*"), ctx);
  EXPECT_FALSE(result.is_true());
}
