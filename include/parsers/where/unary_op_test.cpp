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

#include <boost/shared_ptr.hpp>
#include <list>
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

static node_type make_un_op(operators op, node_type subject) { return factory::create_un_op(op, subject); }

// ======================================================================
// can_evaluate
// ======================================================================

TEST(UnaryOp, CanEvaluateReturnsTrue) {
  auto node = make_un_op(op_not, make_int(1));
  EXPECT_TRUE(node->can_evaluate());
}

// ======================================================================
// to_string
// ======================================================================

TEST(UnaryOp, ToStringWithoutContext) {
  auto node = make_un_op(op_not, make_int(42));
  std::string result = node->to_string();
  EXPECT_NE(result.find("42"), std::string::npos);
}

TEST(UnaryOp, ToStringWithContext) {
  auto ctx = make_context();
  auto node = make_un_op(op_not, make_int(42));
  std::string result = node->to_string(ctx);
  EXPECT_NE(result.find("42"), std::string::npos);
}

// ======================================================================
// evaluate — op_not on bool/int types
// ======================================================================

TEST(UnaryOp, NotOnBoolTrueReturnsFalse) {
  auto ctx = make_context();
  auto subject = make_int(1);
  subject->set_type(type_bool);
  auto node = make_un_op(op_not, subject);
  node->infer_type(make_converter());
  auto result = node->evaluate(ctx);
  EXPECT_EQ(result->get_int_value(ctx), 0);
  EXPECT_FALSE(ctx->has_error());
}

TEST(UnaryOp, NotOnBoolFalseReturnsTrue) {
  auto ctx = make_context();
  auto subject = make_int(0);
  subject->set_type(type_bool);
  auto node = make_un_op(op_not, subject);
  node->infer_type(make_converter());
  auto result = node->evaluate(ctx);
  EXPECT_EQ(result->get_int_value(ctx), 1);
  EXPECT_FALSE(ctx->has_error());
}

TEST(UnaryOp, NotOnIntNegates) {
  auto ctx = make_context();
  auto node = make_un_op(op_not, make_int(5));
  node->infer_type(make_converter());
  auto result = node->evaluate(ctx);
  EXPECT_EQ(result->get_int_value(ctx), -5);
  EXPECT_FALSE(ctx->has_error());
}

TEST(UnaryOp, NotOnIntNegativeBecomesPositive) {
  auto ctx = make_context();
  auto node = make_un_op(op_not, make_int(-3));
  node->infer_type(make_converter());
  auto result = node->evaluate(ctx);
  EXPECT_EQ(result->get_int_value(ctx), 3);
  EXPECT_FALSE(ctx->has_error());
}

TEST(UnaryOp, NotOnIntZeroStaysZero) {
  auto ctx = make_context();
  auto node = make_un_op(op_not, make_int(0));
  node->infer_type(make_converter());
  auto result = node->evaluate(ctx);
  EXPECT_EQ(result->get_int_value(ctx), 0);
  EXPECT_FALSE(ctx->has_error());
}

// ======================================================================
// get_value delegates through evaluate
// ======================================================================

TEST(UnaryOp, GetValueDelegatesToEvaluate) {
  auto ctx = make_context();
  auto node = make_un_op(op_not, make_int(7));
  node->infer_type(make_converter());
  auto vc = node->get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), -7);
}

// ======================================================================
// get_list_value returns empty list
// ======================================================================

TEST(UnaryOp, GetListValueReturnsEmptyList) {
  auto ctx = make_context();
  auto node = make_un_op(op_not, make_int(1));
  auto list = node->get_list_value(ctx);
  EXPECT_TRUE(list.empty());
}

// ======================================================================
// bind delegates to subject
// ======================================================================

TEST(UnaryOp, BindDelegatesToSubject) {
  auto node = make_un_op(op_not, make_int(1));
  EXPECT_TRUE(node->bind(make_converter()));
}

// ======================================================================
// infer_type
// ======================================================================

TEST(UnaryOp, InferTypeNotReturnsBool) {
  auto node = make_un_op(op_not, make_int(42));
  auto result = node->infer_type(make_converter());
  EXPECT_EQ(result, type_bool);
}

TEST(UnaryOp, InferTypeNotWithSuggestionReturnsBool) {
  auto node = make_un_op(op_not, make_int(42));
  auto result = node->infer_type(make_converter(), type_int);
  EXPECT_EQ(result, type_bool);
}

TEST(UnaryOp, InferTypeInvReturnsSubjectType) {
  auto node = make_un_op(op_inv, make_int(42));
  auto result = node->infer_type(make_converter());
  EXPECT_EQ(result, type_int);
}

TEST(UnaryOp, InferTypeInvWithStringSuggestionReturnsString) {
  auto node = make_un_op(op_inv, make_string("hello"));
  auto result = node->infer_type(make_converter());
  EXPECT_EQ(result, type_string);
}

TEST(UnaryOp, InferTypeInvWithFloatSubject) {
  auto node = make_un_op(op_inv, make_float(3.14));
  auto result = node->infer_type(make_converter());
  EXPECT_EQ(result, type_float);
}

// ======================================================================
// static_evaluate delegates to subject
// ======================================================================

TEST(UnaryOp, StaticEvaluateDelegatesToSubject) {
  auto ctx = make_context();
  auto node = make_un_op(op_not, make_int(1));
  EXPECT_TRUE(node->static_evaluate(ctx));
}

// ======================================================================
// require_object delegates to subject
// ======================================================================

TEST(UnaryOp, RequireObjectDelegatesToSubject) {
  auto ctx = make_context();
  auto node = make_un_op(op_not, make_int(1));
  EXPECT_FALSE(node->require_object(ctx));
}

// ======================================================================
// find_performance_data delegates to subject
// ======================================================================

TEST(UnaryOp, FindPerformanceDataDelegatesToSubject) {
  auto ctx = make_context();
  performance_collector collector;
  auto node = make_un_op(op_not, make_int(42));
  bool result = node->find_performance_data(ctx, collector);
  EXPECT_FALSE(result);
  // The int_value subject should have set the candidate value
  EXPECT_TRUE(collector.has_candidate_value());
}

// ======================================================================
// Unhandled operator falls back to create_false
// ======================================================================

TEST(UnaryOp, UnhandledOperatorSetsError) {
  auto ctx = make_context();
  // op_inv has get_return_type returning the subject type (int), which is int,
  // so it will take the int path and use the unary operator impl.
  // But the op_factory::get_unary_operator for op_inv is not op_not,
  // it falls through to operator_false.
  auto node = make_un_op(op_inv, make_int(42));
  node->infer_type(make_converter());
  auto result = node->evaluate(ctx);
  EXPECT_TRUE(ctx->has_error());
}
