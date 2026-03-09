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

#include <boost/make_shared.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/helpers.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/value_node.hpp>

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
// Mock object_converter for infer_type / bind
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
  bool can_convert(std::string, boost::shared_ptr<any_node>, value_type) override { return false; }
  boost::shared_ptr<binary_function_impl> create_converter(std::string, boost::shared_ptr<any_node>, value_type) override { return nullptr; }
};

// ======================================================================
// Helpers
// ======================================================================

static evaluation_context make_context() { return boost::make_shared<mock_evaluation_context>(); }

static object_converter make_converter() { return boost::make_shared<mock_object_converter>(); }

static node_type make_int(long long v) { return factory::create_int(v); }
static node_type make_float(double v) { return factory::create_float(v); }
static node_type make_string(const std::string &v) { return factory::create_string(v); }

static node_type make_bin_op(operators op, node_type lhs, node_type rhs) { return factory::create_bin_op(op, lhs, rhs); }

// ======================================================================
// can_evaluate
// ======================================================================

TEST(BinaryOp, CanEvaluateReturnsTrue) {
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(1));
  EXPECT_TRUE(node->can_evaluate());
}

// ======================================================================
// Integer equality
// ======================================================================

TEST(BinaryOp, EvaluateIntEqualTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_int(42), make_int(42));
  node->infer_type(make_converter());
  const value_container result = node->get_value(ctx, type_int);
  EXPECT_TRUE(result.is_true());
}

TEST(BinaryOp, EvaluateIntEqualFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_int(42), make_int(99));
  node->infer_type(make_converter());
  const value_container result = node->get_value(ctx, type_int);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// Integer not-equal
// ======================================================================

TEST(BinaryOp, EvaluateIntNotEqualTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_ne, make_int(1), make_int(2));
  node->infer_type(make_converter());
  const value_container result = node->get_value(ctx, type_int);
  EXPECT_TRUE(result.is_true());
}

TEST(BinaryOp, EvaluateIntNotEqualFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_ne, make_int(5), make_int(5));
  node->infer_type(make_converter());
  const value_container result = node->get_value(ctx, type_int);
  EXPECT_FALSE(result.is_true());
}

// ======================================================================
// Integer comparisons (gt, lt, ge, le)
// ======================================================================

TEST(BinaryOp, EvaluateIntGreaterThanTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_gt, make_int(10), make_int(5));
  node->infer_type(make_converter());
  const value_container result = node->get_value(ctx, type_int);
  EXPECT_TRUE(result.is_true());
}

TEST(BinaryOp, EvaluateIntGreaterThanFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_gt, make_int(5), make_int(10));
  node->infer_type(make_converter());
  const value_container result = node->get_value(ctx, type_int);
  EXPECT_FALSE(result.is_true());
}

TEST(BinaryOp, EvaluateIntLessThanTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_lt, make_int(3), make_int(7));
  node->infer_type(make_converter());
  const value_container result = node->get_value(ctx, type_int);
  EXPECT_TRUE(result.is_true());
}

TEST(BinaryOp, EvaluateIntLessThanFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_lt, make_int(7), make_int(3));
  node->infer_type(make_converter());
  const value_container result = node->get_value(ctx, type_int);
  EXPECT_FALSE(result.is_true());
}

TEST(BinaryOp, EvaluateIntGreaterOrEqualTrue) {
  const auto ctx = make_context();
  const node_type eq_node = make_bin_op(op_ge, make_int(5), make_int(5));
  eq_node->infer_type(make_converter());
  EXPECT_TRUE(eq_node->get_value(ctx, type_int).is_true());

  const node_type gt_node = make_bin_op(op_ge, make_int(6), make_int(5));
  gt_node->infer_type(make_converter());
  EXPECT_TRUE(gt_node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateIntGreaterOrEqualFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_ge, make_int(4), make_int(5));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateIntLessOrEqualTrue) {
  const auto ctx = make_context();
  const node_type eq_node = make_bin_op(op_le, make_int(5), make_int(5));
  eq_node->infer_type(make_converter());
  EXPECT_TRUE(eq_node->get_value(ctx, type_int).is_true());

  const node_type lt_node = make_bin_op(op_le, make_int(4), make_int(5));
  lt_node->infer_type(make_converter());
  EXPECT_TRUE(lt_node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateIntLessOrEqualFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_le, make_int(6), make_int(5));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}

// ======================================================================
// Logical AND / OR
// ======================================================================

TEST(BinaryOp, EvaluateAndTrueTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_and, make_int(1), make_int(1));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateAndTrueFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_and, make_int(1), make_int(0));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateAndFalseFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_and, make_int(0), make_int(0));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateOrTrueFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_or, make_int(1), make_int(0));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateOrFalseFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_or, make_int(0), make_int(0));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateOrFalseTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_or, make_int(0), make_int(1));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

// ======================================================================
// String equality
// ======================================================================

TEST(BinaryOp, EvaluateStringEqualTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_string("hello"), make_string("hello"));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateStringEqualFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_string("hello"), make_string("world"));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateStringNotEqualTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_ne, make_string("abc"), make_string("def"));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

// ======================================================================
// String like operator
// ======================================================================

TEST(BinaryOp, EvaluateLikeMatchSubstring) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_like, make_string("hello world"), make_string("world"));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateLikeNoMatch) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_like, make_string("hello world"), make_string("xyz"));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateLikeCaseInsensitive) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_like, make_string("Hello World"), make_string("hello"));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

// ======================================================================
// to_string
// ======================================================================

TEST(BinaryOp, ToStringContainsOperatorAndOperands) {
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(2));
  const std::string s = node->to_string();
  EXPECT_EQ(s, "(tbd){(i){1} = (i){2}}");
}

TEST(BinaryOp, ToStringWithContextContainsOperands) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_gt, make_int(10), make_int(5));
  const std::string s = node->to_string(ctx);
  EXPECT_EQ(s, "10 > 5");
}

// ======================================================================
// get_list_value returns empty list
// ======================================================================

TEST(BinaryOp, GetListValueReturnsEmptyList) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(1));
  const auto list = node->get_list_value(ctx);
  EXPECT_TRUE(list.empty());
}

// ======================================================================
// infer_type
// ======================================================================

TEST(BinaryOp, InferTypeIntEqReturnsBool) {
  const auto converter = make_converter();
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(2));
  const value_type result = node->infer_type(converter);
  // op_eq returns type_bool
  EXPECT_EQ(type_bool, result);
  EXPECT_EQ(type_bool, node->get_type());
}

TEST(BinaryOp, InferTypeWithSuggestionDelegates) {
  const auto converter = make_converter();
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(2));
  const value_type result = node->infer_type(converter, type_string);
  // infer_type(converter, suggestion) delegates to infer_type(converter)
  EXPECT_EQ(type_bool, result);
}

TEST(BinaryOp, InferTypeStringEqReturnsBool) {
  const auto converter = make_converter();
  const node_type node = make_bin_op(op_eq, make_string("a"), make_string("b"));
  const value_type result = node->infer_type(converter);
  EXPECT_EQ(type_bool, result);
}

TEST(BinaryOp, InferTypeBinAndReturnsOperandType) {
  const auto converter = make_converter();
  const node_type node = make_bin_op(op_binand, make_int(3), make_int(1));
  const value_type result = node->infer_type(converter);
  // op_binand preserves the operand type (type_int)
  EXPECT_EQ(type_int, result);
}

TEST(BinaryOp, InferTypeBinOrReturnsOperandType) {
  const auto converter = make_converter();
  const node_type node = make_bin_op(op_binor, make_int(3), make_int(1));
  const value_type result = node->infer_type(converter);
  EXPECT_EQ(type_int, result);
}

// ======================================================================
// bind
// ======================================================================

TEST(BinaryOp, BindReturnsTrueForValueNodes) {
  const auto converter = make_converter();
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(2));
  EXPECT_TRUE(node->bind(converter));
}

TEST(BinaryOp, BindReturnsTrueForStringNodes) {
  const auto converter = make_converter();
  const node_type node = make_bin_op(op_eq, make_string("a"), make_string("b"));
  EXPECT_TRUE(node->bind(converter));
}

// ======================================================================
// static_evaluate
// ======================================================================

TEST(BinaryOp, StaticEvaluateReturnsTrueForLiterals) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(2));
  EXPECT_TRUE(node->static_evaluate(ctx));
}

TEST(BinaryOp, StaticEvaluateBothSidesMustBeTrue) {
  const auto ctx = make_context();
  // Two literal ints: both static_evaluate to true, so AND is true
  const node_type node = make_bin_op(op_and, make_int(1), make_int(1));
  EXPECT_TRUE(node->static_evaluate(ctx));
}

// ======================================================================
// require_object
// ======================================================================

TEST(BinaryOp, RequireObjectFalseForLiterals) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(2));
  EXPECT_FALSE(node->require_object(ctx));
}

// ======================================================================
// find_performance_data
// ======================================================================

TEST(BinaryOp, FindPerformanceDataReturnsFalseForInOp) {
  const auto ctx = make_context();
  performance_collector collector;
  // op_in should short-circuit and return false
  const node_type node = make_bin_op(op_in, make_int(1), make_int(2));
  EXPECT_FALSE(node->find_performance_data(ctx, collector));
}

TEST(BinaryOp, FindPerformanceDataReturnsFalseForNotInOp) {
  const auto ctx = make_context();
  performance_collector collector;
  const node_type node = make_bin_op(op_nin, make_int(1), make_int(2));
  EXPECT_FALSE(node->find_performance_data(ctx, collector));
}

TEST(BinaryOp, FindPerformanceDataReturnsFalseForPlainInts) {
  const auto ctx = make_context();
  performance_collector collector;
  // Two plain int values: both set candidate_value but no candidate_variable,
  // so has_candidates() returns false for each sub-collector and the overall
  // result is false.
  const node_type node = make_bin_op(op_eq, make_int(1), make_int(2));
  EXPECT_FALSE(node->find_performance_data(ctx, collector));
}

// ======================================================================
// Nested binary_op
// ======================================================================

TEST(BinaryOp, NestedAndOr) {
  const auto ctx = make_context();
  const auto conv = make_converter();
  // (1 == 1) AND (2 > 1) => true AND true => true
  const node_type left = make_bin_op(op_eq, make_int(1), make_int(1));
  const node_type right = make_bin_op(op_gt, make_int(2), make_int(1));
  left->infer_type(conv);
  right->infer_type(conv);
  const node_type combined = make_bin_op(op_and, left, right);
  combined->infer_type(conv);
  EXPECT_TRUE(combined->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, NestedOrShortCircuit) {
  const auto ctx = make_context();
  const auto conv = make_converter();
  // (1 == 1) OR (2 < 1) => true OR false => true
  const node_type left = make_bin_op(op_eq, make_int(1), make_int(1));
  const node_type right = make_bin_op(op_lt, make_int(2), make_int(1));
  left->infer_type(conv);
  right->infer_type(conv);
  const node_type combined = make_bin_op(op_or, left, right);
  combined->infer_type(conv);
  EXPECT_TRUE(combined->get_value(ctx, type_int).is_true());
}

// ======================================================================
// Float comparisons
// ======================================================================

TEST(BinaryOp, EvaluateFloatEqualTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_float(3.14), make_float(3.14));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateFloatEqualFalse) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_float(3.14), make_float(2.71));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateFloatGreaterThanTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_gt, make_float(3.14), make_float(2.71));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateFloatLessThanTrue) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_lt, make_float(1.5), make_float(2.5));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

// ======================================================================
// Edge cases
// ======================================================================

TEST(BinaryOp, EvaluateIntEqualZeros) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_int(0), make_int(0));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateIntEqualNegativeNumbers) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_int(-5), make_int(-5));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateIntLtNegativeNumbers) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_lt, make_int(-10), make_int(-5));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateStringEqualEmpty) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_eq, make_string(""), make_string(""));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateLikeEmptyStrings) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_like, make_string(""), make_string(""));
  node->infer_type(make_converter());
  EXPECT_TRUE(node->get_value(ctx, type_int).is_true());
}

TEST(BinaryOp, EvaluateLikeEmptyPattern) {
  const auto ctx = make_context();
  const node_type node = make_bin_op(op_like, make_string("hello"), make_string(""));
  node->infer_type(make_converter());
  EXPECT_FALSE(node->get_value(ctx, type_int).is_true());
}
