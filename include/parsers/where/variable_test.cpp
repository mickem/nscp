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

#include <parsers/where/node.hpp>
#include <parsers/where/variable.hpp>
#include <string>

using namespace parsers::where;

// ======================================================================
// Mock object and context for variable node tests
// ======================================================================

struct mock_object {
  long long int_val;
  double float_val;
  std::string str_val;
};

struct mock_summary {
  long long count;
  std::string status;
};

struct mock_variable_context : evaluation_context_interface {
  typedef mock_object object_type;
  typedef mock_summary* summary_type;
  typedef boost::function<std::string(object_type, evaluation_context)> bound_string_type;
  typedef boost::function<long long(object_type, evaluation_context)> bound_int_type;
  typedef boost::function<double(object_type, evaluation_context)> bound_float_type;

  boost::optional<mock_object> object_;
  mock_summary* summary_;
  std::string error_;
  std::string warn_;
  bool debug_enabled_ = false;

  mock_variable_context() : summary_(nullptr) {}

  bool has_object() { return static_cast<bool>(object_); }
  mock_object get_object() { return *object_; }
  void set_object(mock_object obj) { object_ = obj; }
  void remove_object() { object_.reset(); }
  mock_summary* get_summary() { return summary_; }
  void set_summary(mock_summary* s) { summary_ = s; }

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
// Helpers
// ======================================================================

static evaluation_context make_var_context() { return std::make_shared<mock_variable_context>(); }

static evaluation_context make_var_context_with_object(mock_object obj) {
  auto ctx = std::make_shared<mock_variable_context>();
  ctx->set_object(obj);
  return ctx;
}

static mock_variable_context* native(evaluation_context ctx) { return reinterpret_cast<mock_variable_context*>(ctx.get()); }

// ======================================================================
// Mock object_converter for infer_type tests
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

static object_converter make_converter() { return std::make_shared<mock_object_converter>(); }

// ======================================================================
// int_variable_node — construction and type
// ======================================================================

typedef int_variable_node<mock_variable_context> int_var_node;
typedef float_variable_node<mock_variable_context> float_var_node;
typedef str_variable_node<mock_variable_context> str_var_node;
typedef dual_variable_node<mock_variable_context> dual_var_node;

static mock_variable_context::bound_int_type make_int_fun() {
  return [](mock_object obj, evaluation_context) -> long long { return obj.int_val; };
}

static mock_variable_context::bound_float_type make_float_fun() {
  return [](mock_object obj, evaluation_context) -> double { return obj.float_val; };
}

static mock_variable_context::bound_string_type make_str_fun() {
  return [](mock_object obj, evaluation_context) -> std::string { return obj.str_val; };
}

// ======================================================================
// int_variable_node tests
// ======================================================================

TEST(IntVariableNode, TypeIsPreserved) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  EXPECT_EQ(node.get_type(), type_int);
}

TEST(IntVariableNode, CanEvaluateReturnsTrue) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  EXPECT_TRUE(node.can_evaluate());
}

TEST(IntVariableNode, ToStringWithoutContext) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("my_var", type_int, make_int_fun(), perfgen);
  EXPECT_EQ(node.to_string(), "{int}my_var");
}

TEST(IntVariableNode, ToStringWithContextAndObject) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("my_var", type_int, make_int_fun(), perfgen);
  mock_object obj{42, 0.0, ""};
  auto ctx = make_var_context_with_object(obj);
  EXPECT_EQ(node.to_string(ctx), "42");
}

TEST(IntVariableNode, ToStringWithContextNoObject) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("my_var", type_int, make_int_fun(), perfgen);
  auto ctx = make_var_context();
  EXPECT_EQ(node.to_string(ctx), "my_var?");
}

TEST(IntVariableNode, GetValueAsInt) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  mock_object obj{99, 0.0, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 99);
  EXPECT_FALSE(ctx->has_error());
}

TEST(IntVariableNode, GetValueAsFloat) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  mock_object obj{42, 0.0, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_float);
  EXPECT_DOUBLE_EQ(vc.get_float(), 42.0);
  EXPECT_FALSE(ctx->has_error());
}

TEST(IntVariableNode, GetValueNoObjectSetsWarning) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  auto ctx = make_var_context();
  auto vc = node.get_value(ctx, type_int);
  EXPECT_TRUE(ctx->has_warn());
  EXPECT_TRUE(vc.is_unsure);
}

TEST(IntVariableNode, GetValueInvalidTypeSetsError) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  mock_object obj{42, 0.0, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_string);
  EXPECT_TRUE(ctx->has_error());
}

TEST(IntVariableNode, EvaluateReturnsIntNode) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  mock_object obj{77, 0.0, ""};
  auto ctx = make_var_context_with_object(obj);
  auto result = node.evaluate(ctx);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->get_int_value(ctx), 77);
}

TEST(IntVariableNode, EvaluateNoObjectSetsError) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  auto ctx = make_var_context();
  auto result = node.evaluate(ctx);
  EXPECT_TRUE(ctx->has_error());
}

TEST(IntVariableNode, StaticEvaluateReturnsFalse) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  auto ctx = make_var_context();
  EXPECT_FALSE(node.static_evaluate(ctx));
}

TEST(IntVariableNode, RequireObjectReturnsTrue) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  auto ctx = make_var_context();
  EXPECT_TRUE(node.require_object(ctx));
}

TEST(IntVariableNode, BindReturnsTrue) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  EXPECT_TRUE(node.bind(make_converter()));
}

TEST(IntVariableNode, InferTypeWithIntSuggestionKeepsType) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  auto result = node.infer_type(make_converter(), type_int);
  EXPECT_EQ(result, type_int);
}

TEST(IntVariableNode, InferTypeWithFloatSuggestionChanges) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  auto result = node.infer_type(make_converter(), type_float);
  EXPECT_EQ(result, type_float);
}

TEST(IntVariableNode, InferTypeWithoutSuggestionReturnsType) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  EXPECT_EQ(node.infer_type(make_converter()), type_int);
}

TEST(IntVariableNode, FindPerformanceDataSetsCandidateVariable) {
  std::list<int_var_node::int_performance_generator> perfgen;
  int_var_node node("test_int", type_int, make_int_fun(), perfgen);
  auto ctx = make_var_context();
  performance_collector collector;
  bool result = node.find_performance_data(ctx, collector);
  EXPECT_FALSE(result);
  EXPECT_TRUE(collector.has_candidate_variable());
  EXPECT_EQ(collector.get_variable(), "test_int");
}

// ======================================================================
// float_variable_node tests
// ======================================================================

TEST(FloatVariableNode, TypeIsPreserved) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  EXPECT_EQ(node.get_type(), type_float);
}

TEST(FloatVariableNode, CanEvaluateReturnsTrue) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  EXPECT_TRUE(node.can_evaluate());
}

TEST(FloatVariableNode, ToStringWithoutContext) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("my_float", type_float, make_float_fun(), perfgen);
  EXPECT_EQ(node.to_string(), "{float}my_float");
}

TEST(FloatVariableNode, ToStringWithContextAndObject) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("my_float", type_float, make_float_fun(), perfgen);
  mock_object obj{0, 3.14, ""};
  auto ctx = make_var_context_with_object(obj);
  std::string result = node.to_string(ctx);
  EXPECT_FALSE(result.empty());
}

TEST(FloatVariableNode, ToStringWithContextNoObject) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("my_float", type_float, make_float_fun(), perfgen);
  auto ctx = make_var_context();
  EXPECT_EQ(node.to_string(ctx), "(float)var:my_float");
}

TEST(FloatVariableNode, GetValueAsFloat) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  mock_object obj{0, 2.718, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_float);
  EXPECT_DOUBLE_EQ(vc.get_float(), 2.718);
  EXPECT_FALSE(ctx->has_error());
}

TEST(FloatVariableNode, GetValueAsInt) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  mock_object obj{0, 7.9, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 7);
  EXPECT_FALSE(ctx->has_error());
}

TEST(FloatVariableNode, GetValueNoObjectSetsWarning) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  auto ctx = make_var_context();
  auto vc = node.get_value(ctx, type_float);
  EXPECT_TRUE(ctx->has_warn());
  EXPECT_TRUE(vc.is_unsure);
}

TEST(FloatVariableNode, GetValueInvalidTypeSetsError) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  mock_object obj{0, 1.0, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_string);
  EXPECT_TRUE(ctx->has_error());
}

TEST(FloatVariableNode, EvaluateReturnsFloatNode) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  mock_object obj{0, 1.5, ""};
  auto ctx = make_var_context_with_object(obj);
  auto result = node.evaluate(ctx);
  ASSERT_NE(result, nullptr);
  EXPECT_DOUBLE_EQ(result->get_float_value(ctx), 1.5);
}

TEST(FloatVariableNode, EvaluateNoObjectSetsError) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  auto ctx = make_var_context();
  auto result = node.evaluate(ctx);
  EXPECT_TRUE(ctx->has_error());
}

TEST(FloatVariableNode, StaticEvaluateReturnsFalse) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  auto ctx = make_var_context();
  EXPECT_FALSE(node.static_evaluate(ctx));
}

TEST(FloatVariableNode, RequireObjectReturnsTrue) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  auto ctx = make_var_context();
  EXPECT_TRUE(node.require_object(ctx));
}

TEST(FloatVariableNode, InferTypeWithIntSuggestionChanges) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  auto result = node.infer_type(make_converter(), type_int);
  EXPECT_EQ(result, type_int);
}

TEST(FloatVariableNode, InferTypeWithoutSuggestionReturnsFloat) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  EXPECT_EQ(node.infer_type(make_converter()), type_float);
}

TEST(FloatVariableNode, FindPerformanceDataSetsCandidateVariable) {
  std::list<float_var_node::float_performance_generator> perfgen;
  float_var_node node("test_float", type_float, make_float_fun(), perfgen);
  auto ctx = make_var_context();
  performance_collector collector;
  bool result = node.find_performance_data(ctx, collector);
  EXPECT_FALSE(result);
  EXPECT_TRUE(collector.has_candidate_variable());
  EXPECT_EQ(collector.get_variable(), "test_float");
}

// ======================================================================
// str_variable_node tests
// ======================================================================

TEST(StrVariableNode, TypeIsPreserved) {
  str_var_node node("test_str", type_string, make_str_fun());
  EXPECT_EQ(node.get_type(), type_string);
}

TEST(StrVariableNode, CanEvaluateReturnsTrue) {
  str_var_node node("test_str", type_string, make_str_fun());
  EXPECT_TRUE(node.can_evaluate());
}

TEST(StrVariableNode, ToStringWithoutContext) {
  str_var_node node("my_str", type_string, make_str_fun());
  EXPECT_EQ(node.to_string(), "{string}my_str");
}

TEST(StrVariableNode, ToStringWithContextAndObject) {
  str_var_node node("my_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "hello"};
  auto ctx = make_var_context_with_object(obj);
  EXPECT_EQ(node.to_string(ctx), "hello");
}

TEST(StrVariableNode, ToStringWithContextNoObject) {
  str_var_node node("my_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  EXPECT_EQ(node.to_string(ctx), "(string)var:my_str");
}

TEST(StrVariableNode, GetValueAsString) {
  str_var_node node("test_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "world"};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_string);
  EXPECT_EQ(vc.get_string(), "world");
  EXPECT_FALSE(ctx->has_error());
}

TEST(StrVariableNode, GetValueInvalidTypeSetsError) {
  str_var_node node("test_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "hello"};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_int);
  EXPECT_TRUE(ctx->has_error());
}

TEST(StrVariableNode, GetIntValueSetsError) {
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  long long result = node.get_int_value(ctx);
  EXPECT_EQ(result, 0);
  EXPECT_TRUE(ctx->has_error());
}

TEST(StrVariableNode, GetFloatValueSetsError) {
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  double result = node.get_float_value(ctx);
  EXPECT_DOUBLE_EQ(result, 0.0);
  EXPECT_TRUE(ctx->has_error());
}

TEST(StrVariableNode, GetValueNoObjectReturnsEmptyUnsureWithWarn) {
  // Match the contract used by int_variable_node / float_variable_node: when
  // there's no current object, return a typed default (empty string here)
  // with is_unsure=true and a WARN (not ERROR). Centralises the unsure
  // propagation so downstream operators don't need per-operator nil-guards.
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  auto vc = node.get_value(ctx, type_string);
  EXPECT_FALSE(ctx->has_error()) << "no-object should warn, not error: " << ctx->get_error();
  EXPECT_TRUE(ctx->has_warn());
  EXPECT_TRUE(vc.is(type_string));
  EXPECT_EQ(vc.get_string(), "");
  EXPECT_TRUE(vc.is_unsure);
}

TEST(StrVariableNode, EvaluateReturnsStringNode) {
  str_var_node node("test_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "evaluated"};
  auto ctx = make_var_context_with_object(obj);
  auto result = node.evaluate(ctx);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->get_string_value(ctx), "evaluated");
}

TEST(StrVariableNode, EvaluateNoObjectSetsWarn) {
  // Demoted from error → warn so production agent logs are not flooded
  // with ERROR-level entries on every empty-rows force-evaluate tick.
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  auto result = node.evaluate(ctx);
  EXPECT_FALSE(ctx->has_error()) << "no-object evaluate should warn, not error";
  EXPECT_TRUE(ctx->has_warn());
}

TEST(StrVariableNode, StaticEvaluateReturnsFalse) {
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  EXPECT_FALSE(node.static_evaluate(ctx));
}

TEST(StrVariableNode, RequireObjectReturnsTrue) {
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  EXPECT_TRUE(node.require_object(ctx));
}

TEST(StrVariableNode, InferTypeAlwaysReturnsString) {
  str_var_node node("test_str", type_string, make_str_fun());
  EXPECT_EQ(node.infer_type(make_converter()), type_string);
  EXPECT_EQ(node.infer_type(make_converter(), type_int), type_string);
}

TEST(StrVariableNode, FindPerformanceDataSetsCandidateVariable) {
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  performance_collector collector;
  bool result = node.find_performance_data(ctx, collector);
  EXPECT_FALSE(result);
  EXPECT_TRUE(collector.has_candidate_variable());
  EXPECT_EQ(collector.get_variable(), "test_str");
}

// ======================================================================
// dual_variable_node tests
// ======================================================================

TEST(DualVariableNode, TypeIsMulti) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  EXPECT_EQ(node.get_type(), type_multi);
}

TEST(DualVariableNode, CanEvaluateReturnsTrue) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  EXPECT_TRUE(node.can_evaluate());
}

TEST(DualVariableNode, ToStringWithoutContextInt) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("my_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  // type is type_multi initially, so to_string depends on inferred type
  EXPECT_EQ(node.to_string(), "{unknown:88}my_dual");
}

TEST(DualVariableNode, ToStringWithoutContextAfterInferInt) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("my_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  node.infer_type(make_converter(), type_int);
  EXPECT_EQ(node.to_string(), "{int}my_dual");
}

TEST(DualVariableNode, ToStringWithoutContextAfterInferString) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("my_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  node.infer_type(make_converter(), type_string);
  EXPECT_EQ(node.to_string(), "{string}my_dual");
}

TEST(DualVariableNode, ToStringWithContextAndObjectUsesStringFun) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("my_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  mock_object obj{42, 0.0, "hello"};
  auto ctx = make_var_context_with_object(obj);
  EXPECT_EQ(node.to_string(ctx), "hello");
}

TEST(DualVariableNode, ToStringWithContextNoObject) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("my_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto ctx = make_var_context();
  std::string result = node.to_string(ctx);
  EXPECT_EQ(result, "my_dual?");
}

TEST(DualVariableNode, GetValueAsIntWithObject) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  mock_object obj{55, 0.0, "text"};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 55);
  EXPECT_FALSE(ctx->has_error());
}

TEST(DualVariableNode, GetValueAsStringWithObject) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  mock_object obj{42, 0.0, "dual_str"};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_string);
  EXPECT_EQ(vc.get_string(), "dual_str");
  EXPECT_FALSE(ctx->has_error());
}

TEST(DualVariableNode, GetValueNoObjectSetsWarning) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto ctx = make_var_context();
  auto vc = node.get_value(ctx, type_int);
  EXPECT_TRUE(ctx->has_warn());
  EXPECT_TRUE(vc.is_unsure);
}

TEST(DualVariableNode, GetValueNoObjectStringReturnsEmptyUnsure) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto ctx = make_var_context();
  auto vc = node.get_value(ctx, type_string);
  EXPECT_TRUE(ctx->has_warn());
  EXPECT_TRUE(vc.is_unsure);
  EXPECT_EQ(vc.get_string(), "");
}

TEST(DualVariableNode, EvaluateAsIntWithObject) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  // set type to int for evaluation path
  node.infer_type(make_converter(), type_int);
  // but evaluate as int path (not string, not float)
  mock_object obj{88, 0.0, "text"};
  auto ctx = make_var_context_with_object(obj);
  auto result = node.evaluate(ctx);
  // TODO: This test needs fixing...
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(ctx->has_error());
  EXPECT_EQ("Failed to evaluate test_dual no object instance", ctx->get_error());
}

TEST(DualVariableNode, EvaluateAsStringWithObject) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  node.infer_type(make_converter(), type_string);
  mock_object obj{0, 0.0, "eval_str"};
  auto ctx = make_var_context_with_object(obj);
  auto result = node.evaluate(ctx);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->get_string_value(ctx), "eval_str");
}

TEST(DualVariableNode, EvaluateNoObjectSetsError) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto ctx = make_var_context();
  auto result = node.evaluate(ctx);
  EXPECT_TRUE(ctx->has_error());
}

TEST(DualVariableNode, StaticEvaluateReturnsFalse) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto ctx = make_var_context();
  EXPECT_FALSE(node.static_evaluate(ctx));
}

TEST(DualVariableNode, RequireObjectReturnsTrue) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto ctx = make_var_context();
  EXPECT_TRUE(node.require_object(ctx));
}

TEST(DualVariableNode, InferTypeWithIntSuggestion) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto result = node.infer_type(make_converter(), type_int);
  EXPECT_EQ(result, type_int);
  EXPECT_EQ(node.get_type(), type_int);
}

TEST(DualVariableNode, InferTypeWithStringSuggestion) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto result = node.infer_type(make_converter(), type_string);
  EXPECT_EQ(result, type_string);
  EXPECT_EQ(node.get_type(), type_string);
}

TEST(DualVariableNode, InferTypeWithFloatSuggestion) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto result = node.infer_type(make_converter(), type_float);
  EXPECT_EQ(result, type_float);
  EXPECT_EQ(node.get_type(), type_float);
}

TEST(DualVariableNode, InferTypeWithTbdUsesFallback) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto result = node.infer_type(make_converter(), type_tbd);
  EXPECT_EQ(result, type_int);
  EXPECT_EQ(node.get_type(), type_int);
}

TEST(DualVariableNode, InferTypeWithoutSuggestion) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  EXPECT_EQ(node.infer_type(make_converter()), type_multi);
}

TEST(DualVariableNode, FindPerformanceDataSetsCandidateForNonString) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  auto ctx = make_var_context();
  performance_collector collector;
  node.find_performance_data(ctx, collector);
  EXPECT_TRUE(collector.has_candidate_variable());
  EXPECT_EQ(collector.get_variable(), "test_dual");
}

TEST(DualVariableNode, FindPerformanceDataDoesNotSetCandidateForString) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual", type_int, make_int_fun(), make_str_fun(), perfgen);
  node.infer_type(make_converter(), type_string);
  auto ctx = make_var_context();
  performance_collector collector;
  node.find_performance_data(ctx, collector);
  EXPECT_FALSE(collector.has_candidate_variable());
}

// ======================================================================
// dual_variable_node with int + float constructor
// ======================================================================

TEST(DualVariableNodeIntFloat, GetValueAsFloat) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual_if", type_float, make_int_fun(), make_float_fun(), perfgen);
  mock_object obj{10, 3.14, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_float);
  EXPECT_DOUBLE_EQ(vc.get_float(), 3.14);
  EXPECT_FALSE(ctx->has_error());
}

TEST(DualVariableNodeIntFloat, GetValueAsIntUsesIntFun) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual_if", type_float, make_int_fun(), make_float_fun(), perfgen);
  mock_object obj{42, 3.14, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 42);
  EXPECT_FALSE(ctx->has_error());
}

TEST(DualVariableNodeIntFloat, GetValueAsStringFallsBackToFloat) {
  std::list<dual_var_node::int_performance_generator> perfgen;
  dual_var_node node("test_dual_if", type_float, make_int_fun(), make_float_fun(), perfgen);
  mock_object obj{10, 2.5, ""};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_string);
  // Should convert int to string since no s_fun and i_fun exists and is_int() is false (type_multi)
  // Actually with f_fun and no s_fun, it should fall through to f_fun conversion
  EXPECT_FALSE(vc.get_string().empty());
  EXPECT_FALSE(ctx->has_error());
}

// ======================================================================
// custom_function_node tests
// ======================================================================

TEST(CustomFunctionNode, TypeIsPreserved) {
  auto fun = [](const value_type vt, evaluation_context, const node_type subject) -> node_type { return factory::create_string("result"); };
  node_type subject = factory::create_string("input");
  custom_function_node node("my_func", fun, subject, type_string);
  EXPECT_EQ(node.get_type(), type_string);
}

TEST(CustomFunctionNode, CanEvaluateReturnsFalse) {
  auto fun = [](const value_type vt, evaluation_context, const node_type subject) -> node_type { return factory::create_string("result"); };
  node_type subject = factory::create_string("input");
  custom_function_node node("my_func", fun, subject, type_string);
  EXPECT_FALSE(node.can_evaluate());
}

TEST(CustomFunctionNode, ToStringWithoutContext) {
  auto fun = [](const value_type vt, evaluation_context, const node_type subject) -> node_type { return factory::create_string("result"); };
  node_type subject = factory::create_string("input");
  custom_function_node node("my_func", fun, subject, type_string);
  EXPECT_EQ(node.to_string(), "{string}my_func()");
}

TEST(CustomFunctionNode, EvaluateCallsFunction) {
  auto fun = [](const value_type vt, evaluation_context ctx, const node_type subject) -> node_type { return factory::create_string("computed"); };
  node_type subject = factory::create_string("input");
  custom_function_node node("my_func", fun, subject, type_string);
  auto ctx = make_var_context();
  auto result = node.evaluate(ctx);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->get_string_value(ctx), "computed");
}

TEST(CustomFunctionNode, EvaluateNoFunctionSetsError) {
  custom_function_node::bound_function_type no_fun;
  node_type subject = factory::create_string("input");
  custom_function_node node("my_func", no_fun, subject, type_string);
  auto ctx = make_var_context();
  auto result = node.evaluate(ctx);
  EXPECT_TRUE(ctx->has_error());
}

TEST(CustomFunctionNode, StaticEvaluateReturnsFalse) {
  auto fun = [](const value_type vt, evaluation_context, const node_type subject) -> node_type { return factory::create_string("result"); };
  node_type subject = factory::create_string("input");
  custom_function_node node("my_func", fun, subject, type_string);
  auto ctx = make_var_context();
  EXPECT_FALSE(node.static_evaluate(ctx));
}

TEST(CustomFunctionNode, RequireObjectReturnsTrue) {
  auto fun = [](const value_type vt, evaluation_context, const node_type subject) -> node_type { return factory::create_string("result"); };
  node_type subject = factory::create_string("input");
  custom_function_node node("my_func", fun, subject, type_string);
  auto ctx = make_var_context();
  EXPECT_TRUE(node.require_object(ctx));
}

TEST(CustomFunctionNode, InferTypeReturnsString) {
  auto fun = [](const value_type vt, evaluation_context, const node_type subject) -> node_type { return factory::create_string("result"); };
  node_type subject = factory::create_string("input");
  custom_function_node node("my_func", fun, subject, type_string);
  EXPECT_EQ(node.infer_type(make_converter()), type_string);
  EXPECT_EQ(node.infer_type(make_converter(), type_int), type_string);
}

// ======================================================================
// summary_int_variable_node tests
//
// Pre-dd8024ae (the deferred-evaluation refactor) the variable used to
// flag results as is_unsure when an object was set, plus a "X is most
// likely mutating" warn, on the assumption that warn/crit ran during
// iteration with a running summary count. After dd8024ae the warn/crit
// engines run in evaluate_deferred_records() *after* iteration, so the
// summary value is final whenever the variable is consulted from a
// warn/crit predicate.
//
// The heuristic was therefore dropped — these tests pin the post-fix
// contract: sure-int regardless of object presence, no warn.
// ======================================================================

typedef summary_int_variable_node<mock_variable_context> summary_int_var_node;

static mock_variable_context::summary_type set_summary_int(mock_summary& s, long long count) {
  s.count = count;
  return &s;
}

TEST(SummaryIntVariableNode, GetValueWithoutObjectIsSure) {
  // Force-evaluate path (no object set): summary value flows through with
  // is_unsure=false. Pre-fix: also sure but for the heuristic reason
  // ("summary=true → !summary=false").
  summary_int_var_node node("count", [](mock_summary* s) -> long long { return s ? s->count : 0; });
  mock_summary summary;
  auto ctx = make_var_context();
  native(ctx)->set_summary(set_summary_int(summary, 7));

  const auto vc = node.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 7);
  EXPECT_FALSE(vc.is_unsure);
  EXPECT_FALSE(ctx->has_warn()) << "no-object case must not warn: " << ctx->get_warn();
}

TEST(SummaryIntVariableNode, GetValueWithObjectIsAlsoSure) {
  // Deferred per-row replay path (object set): post-fix the result is
  // STILL sure-int with no warn. Pre-fix: this was unsure-int with a
  // "X is most likely mutating" warn — the noisy production-log symptom
  // that motivated the change.
  summary_int_var_node node("count", [](mock_summary* s) -> long long { return s ? s->count : 0; });
  mock_summary summary;
  auto ctx = make_var_context();
  native(ctx)->set_summary(set_summary_int(summary, 5));
  native(ctx)->set_object(mock_object{0, 0.0, ""});

  const auto vc = node.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 5);
  EXPECT_FALSE(vc.is_unsure)
      << "post-dd8024ae: summary is final at deferred-eval time; the legacy "
         "is_unsure-when-object-set heuristic is gone.";
  EXPECT_FALSE(ctx->has_warn())
      << "post-dd8024ae: 'is most likely mutating' warn is gone (was: 1 warn "
         "per row per check tick in production logs).";
}

TEST(SummaryIntVariableNode, EvaluateProducesIntNode) {
  summary_int_var_node node("count", [](mock_summary* s) -> long long { return s ? s->count : 0; });
  mock_summary summary;
  auto ctx = make_var_context();
  native(ctx)->set_summary(set_summary_int(summary, 42));

  auto result = node.evaluate(ctx);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->get_int_value(ctx), 42);
}

TEST(SummaryIntVariableNode, EvaluateNoFunctionSetsError) {
  summary_int_var_node::function_type empty_fun;
  summary_int_var_node node("count", empty_fun);
  auto ctx = make_var_context();

  auto result = node.evaluate(ctx);
  EXPECT_TRUE(ctx->has_error());
}

TEST(SummaryIntVariableNode, RequireObjectIsFalse) {
  // Summary variables are evaluable from summary state alone — they do
  // NOT require an object instance. This is what lets the modern_filter
  // expect_object=false path evaluate `crit=count=0` directly.
  summary_int_var_node node("count", [](mock_summary*) -> long long { return 0; });
  auto ctx = make_var_context();
  EXPECT_FALSE(node.require_object(ctx));
}

TEST(SummaryIntVariableNode, GetValueWithInvalidTypeReturnsNilWithError) {
  summary_int_var_node node("count", [](mock_summary*) -> long long { return 0; });
  auto ctx = make_var_context();
  const auto vc = node.get_value(ctx, type_string);
  EXPECT_TRUE(ctx->has_error());
}

TEST(SummaryIntVariableNode, ToStringWithSummaryProducesValueOnly) {
  // Pre-fix: when object was set, to_string appended "?" to indicate
  // unsure. Post-fix: just the value, regardless of object presence.
  summary_int_var_node node("count", [](mock_summary* s) -> long long { return s ? s->count : 0; });
  mock_summary summary;
  auto ctx = make_var_context();
  native(ctx)->set_summary(set_summary_int(summary, 3));
  native(ctx)->set_object(mock_object{0, 0.0, ""});

  EXPECT_EQ(node.to_string(ctx), "3");
}
