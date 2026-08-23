// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
  boost::optional<long long> opt_val;
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

TEST(StrVariableNode, GetValueNumericParsesTheRowValue) {
  // Numeric reads parse the row's string: this is what answers a comparison
  // against a numeric literal in the number domain.
  str_var_node node("test_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "42"};
  auto ctx = make_var_context_with_object(obj);
  EXPECT_EQ(node.get_value(ctx, type_int).get_int(), 42);
  EXPECT_DOUBLE_EQ(node.get_value(ctx, type_float).get_float(), 42.0);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_FALSE(ctx->has_warn());
}

TEST(StrVariableNode, GetValueNumericParsesDecimals) {
  str_var_node node("test_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "9.5"};
  auto ctx = make_var_context_with_object(obj);
  EXPECT_DOUBLE_EQ(node.get_value(ctx, type_float).get_float(), 9.5);
  EXPECT_FALSE(ctx->has_error());
}

TEST(StrVariableNode, GetValueNumericNonNumberIsNoValueWithWarn) {
  // A row value that is not a number is a certain "nothing to compare": the
  // operators turn a no_value into sure-false (never unsure), and the node
  // warns once so the log points at the offending value.
  str_var_node node("test_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "hello"};
  auto ctx = make_var_context_with_object(obj);
  const auto vc = node.get_value(ctx, type_int);
  EXPECT_TRUE(vc.is_no_value);
  EXPECT_FALSE(vc.is_unsure);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(ctx->has_warn());
}

TEST(StrVariableNode, GetValueNumericWarnsOnlyOnce) {
  // One warn per parsed filter, not one per row.
  str_var_node node("test_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "hello"};
  auto ctx = make_var_context_with_object(obj);
  node.get_value(ctx, type_int);
  const std::string first = ctx->get_warn();
  node.get_value(ctx, type_int);
  EXPECT_EQ(ctx->get_warn(), first);
}

TEST(StrVariableNode, GetValueInvalidTypeSetsError) {
  str_var_node node("test_str", type_string, make_str_fun());
  mock_object obj{0, 0.0, "hello"};
  auto ctx = make_var_context_with_object(obj);
  auto vc = node.get_value(ctx, type_invalid);
  EXPECT_TRUE(ctx->has_error());
}

TEST(StrVariableNode, GetIntValueNoObjectWarnsAndReturnsUnsureDefault) {
  // The numeric accessors follow the same no-object contract as the numeric
  // variable nodes: warn (not error) plus a typed default.
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  EXPECT_EQ(node.get_int_value(ctx), 0);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(ctx->has_warn());
}

TEST(StrVariableNode, GetFloatValueNoObjectWarnsAndReturnsUnsureDefault) {
  str_var_node node("test_str", type_string, make_str_fun());
  auto ctx = make_var_context();
  EXPECT_DOUBLE_EQ(node.get_float_value(ctx), 0.0);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_TRUE(ctx->has_warn());
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

TEST(StrVariableNode, InferTypeNumericSuggestionMovesToFloatDomain) {
  // A bare numeric literal on the other side of a comparison re-types the
  // variable to float ("numbers win"); the row value is parsed per
  // evaluation. Everything else keeps the string domain.
  str_var_node node("test_str", type_string, make_str_fun());
  EXPECT_EQ(node.infer_type(make_converter()), type_string);
  EXPECT_EQ(node.infer_type(make_converter(), type_string), type_string);
  EXPECT_EQ(node.infer_type(make_converter(), type_int), type_float);
  EXPECT_EQ(node.get_type(), type_float);
}

TEST(StrVariableNode, InferTypeFloatSuggestionMovesToFloatDomain) {
  str_var_node node("test_str", type_string, make_str_fun());
  EXPECT_EQ(node.infer_type(make_converter(), type_float), type_float);
  EXPECT_EQ(node.get_type(), type_float);
}

TEST(StrVariableNode, InferTypeNonPlainNumericSuggestionKeepsString) {
  // Only plain int/float pull the variable into the number domain — date,
  // size and custom types keep their existing conversion behaviour.
  str_var_node node("test_str", type_string, make_str_fun());
  EXPECT_EQ(node.infer_type(make_converter(), type_date), type_string);
  EXPECT_EQ(node.infer_type(make_converter(), type_size), type_string);
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
  mock_object obj{88, 0.0, "text"};
  auto ctx = make_var_context_with_object(obj);
  auto result = node.evaluate(ctx);
  // is_float() is lattice-true for type_int, so evaluation goes through the
  // float branch; with no f_fun it is served from the int accessor. This
  // used to error ("no object instance") despite the object being present.
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->get_int_value(ctx), 88);
  EXPECT_FALSE(ctx->has_error()) << ctx->get_error();
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
  EXPECT_FALSE(vc.is_unsure) << "post-dd8024ae: summary is final at deferred-eval time; the legacy "
                                "is_unsure-when-object-set heuristic is gone.";
  EXPECT_FALSE(ctx->has_warn()) << "post-dd8024ae: 'is most likely mutating' warn is gone (was: 1 warn "
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

// ======================================================================
// optional_int_variable_node
// ======================================================================

namespace {
typedef optional_int_variable_node<mock_variable_context> opt_node_type;

std::shared_ptr<opt_node_type> make_opt_node() {
  return std::make_shared<opt_node_type>(
      "ovar", type_int, [](mock_object o, evaluation_context) { return o.opt_val; }, "unknown", std::list<opt_node_type::int_performance_generator>{});
}

mock_object with_opt(long long v) {
  mock_object o{};
  o.opt_val = v;
  return o;
}

mock_object without_opt() { return mock_object{}; }
}  // namespace

TEST(OptionalIntVariableNode, GetValueIntWhenSet) {
  auto node = make_opt_node();
  auto ctx = make_var_context_with_object(with_opt(42));
  const value_container v = node->get_value(ctx, type_int);
  EXPECT_TRUE(v.is(type_int));
  EXPECT_EQ(42, v.get_int());
  EXPECT_FALSE(v.is_no_value);
  EXPECT_FALSE(v.is_unsure);
}

TEST(OptionalIntVariableNode, GetValueIntWhenUnsetIsNoValue) {
  auto node = make_opt_node();
  auto ctx = make_var_context_with_object(without_opt());
  const value_container v = node->get_value(ctx, type_int);
  // Typed (a neutral 0 is carried so stray get_int() callers do not throw)
  // but flagged: the operators treat it as incomparable, not as 0.
  EXPECT_TRUE(v.is(type_int));
  EXPECT_TRUE(v.is_no_value);
  EXPECT_FALSE(v.is_unsure);
  EXPECT_FALSE(native(ctx)->has_error());
}

TEST(OptionalIntVariableNode, GetValueFloatWhenUnsetIsNoValue) {
  auto node = make_opt_node();
  auto ctx = make_var_context_with_object(without_opt());
  const value_container v = node->get_value(ctx, type_float);
  EXPECT_TRUE(v.is(type_float));
  EXPECT_TRUE(v.is_no_value);
}

TEST(OptionalIntVariableNode, GetValueStringRendersNumberOrNoValueString) {
  auto node = make_opt_node();
  auto set_ctx = make_var_context_with_object(with_opt(7));
  EXPECT_EQ("7", node->get_value(set_ctx, type_string).get_string());
  auto unset_ctx = make_var_context_with_object(without_opt());
  const value_container v = node->get_value(unset_ctx, type_string);
  EXPECT_EQ("unknown", v.get_string());
  // The string form is a real string, not a no-value: `jitter = 'unknown'`
  // must compare it, not short-circuit to false.
  EXPECT_FALSE(v.is_no_value);
}

TEST(OptionalIntVariableNode, ToStringRendersNumberOrNoValueString) {
  auto node = make_opt_node();
  auto set_ctx = make_var_context_with_object(with_opt(1234));
  EXPECT_EQ("1234", node->to_string(set_ctx));
  auto unset_ctx = make_var_context_with_object(without_opt());
  EXPECT_EQ("unknown", node->to_string(unset_ctx));
}

TEST(OptionalIntVariableNode, NoObjectIsUnsureNotNoValue) {
  // No current object (the no-rows force-evaluate path) is a different
  // condition from "the object has no value": it stays unsure so match_post
  // can escalate, exactly like the plain int node.
  auto node = make_opt_node();
  auto ctx = make_var_context();
  const value_container v = node->get_value(ctx, type_int);
  EXPECT_TRUE(v.is_unsure);
  EXPECT_FALSE(v.is_no_value);
}

TEST(OptionalIntVariableNode, InferTypeRoutesStringSuggestionToString) {
  auto node = make_opt_node();
  auto converter = make_converter();
  EXPECT_EQ(type_string, node->infer_type(converter, type_string));
  EXPECT_EQ(type_string, node->get_type());
  EXPECT_EQ(type_int, node->infer_type(converter, type_int));
  EXPECT_EQ(type_int, node->get_type());
}

// ======================================================================
// Cross-type comparisons: a variable of one type against a literal of
// another.
//
// The engine derives the type of a comparison in helpers::infer_binary_type.
// The rules, in order:
//  - numbers win: a comparison between a string side and a plain numeric
//    side is answered in the float domain when the string side can join
//    (a string variable parses its row value; a quoted literal joins only
//    if it parses as a number). Operand order does not matter.
//  - a QUOTED literal against a string variable is a string comparison
//    (both sides are type_string before the rule above is consulted), so
//    `version < '8'` stays lexical - quoting is how text ordering is asked
//    for explicitly.
//  - mixed int/float comparisons are answered in float; a fixed-int node
//    (summary counters) is wrapped in a lossless convert instead of the
//    float side being rounded.
//  - registered custom converters (state names, durations) keep precedence,
//    and `like`/`regexp`/`in` keep their string-oriented behaviour.
// ======================================================================

namespace {

// The outcome of running one comparison through the same steps the engine
// runs: derive types (this is where the comparison picks its domain), bind
// (conversion nodes only get their function here), then evaluate.
struct cmp_outcome {
  bool truth;
  bool is_unsure;
  std::string tree;         // typed AST, e.g. {bool}({string}svar = {string}convert(9))
  std::string infer_error;  // errors raised while deriving types
  std::string eval_error;   // errors raised while evaluating
};

cmp_outcome run_cmp(const node_type &left, const operators op, const node_type &right, const evaluation_context &ctx) {
  const node_type expr = factory::create_bin_op(op, left, right);
  const object_converter converter = make_converter();
  expr->infer_type(converter);
  expr->bind(converter);
  cmp_outcome out;
  out.tree = expr->to_string();
  out.infer_error = converter->get_error();
  const value_container result = expr->evaluate(ctx)->get_value(ctx, type_int);
  out.truth = result.get_int(0) != 0;
  out.is_unsure = result.is_unsure;
  out.eval_error = ctx->get_error();
  return out;
}

node_type new_int_var() { return std::make_shared<int_var_node>("ivar", type_int, make_int_fun(), std::list<int_var_node::int_performance_generator>()); }
node_type new_float_var() {
  return std::make_shared<float_var_node>("fvar", type_float, make_float_fun(), std::list<float_var_node::float_performance_generator>());
}
node_type new_str_var() { return std::make_shared<str_var_node>("svar", type_string, make_str_fun()); }
node_type new_dual_int_str_var() {
  return std::make_shared<dual_var_node>("dvar", type_int, make_int_fun(), make_str_fun(), std::list<dual_var_node::int_performance_generator>());
}
node_type new_summary_count_var() {
  return std::make_shared<summary_int_var_node>("count", [](mock_summary *s) -> long long { return s ? s->count : 0; });
}

evaluation_context ctx_with(long long i, double f, const std::string &s) { return make_var_context_with_object(mock_object{i, f, s}); }

evaluation_context ctx_with_count(mock_summary &summary, long long count) {
  auto ctx = make_var_context();
  summary.count = count;
  native(ctx)->set_summary(&summary);
  return ctx;
}

}  // namespace

// ----------------------------------------------------------------------
// int variable vs string literal - the literal joins the variable's domain
// ----------------------------------------------------------------------

TEST(VariableCrossType, IntVarVsNumericStringComparesAsNumbers) {
  // The quoted literal parses as a number, so it joins the numeric domain
  // directly (it used to be wrapped in a convert-to-int node instead).
  const cmp_outcome eq = run_cmp(new_int_var(), op_eq, factory::create_string("42"), ctx_with(42, 0.0, ""));
  EXPECT_TRUE(eq.truth);
  EXPECT_EQ("", eq.eval_error);
  EXPECT_EQ("{bool}({int}ivar = \"42\")", eq.tree);
}

TEST(VariableCrossType, IntVarVsNumericStringIsNotAStringCompare) {
  // 42 > 9 is true as numbers; as text "42" > "9" would be false. The
  // literal is pulled into the variable's int domain, so numbers win.
  const cmp_outcome gt = run_cmp(new_int_var(), op_gt, factory::create_string("9"), ctx_with(42, 0.0, ""));
  EXPECT_TRUE(gt.truth);
  EXPECT_EQ("", gt.eval_error);
}

TEST(VariableCrossType, IntVarVsNonNumericStringIsAnError) {
  const cmp_outcome eq = run_cmp(new_int_var(), op_eq, factory::create_string("abc"), ctx_with(42, 0.0, ""));
  EXPECT_FALSE(eq.truth);
  EXPECT_NE("", eq.eval_error);
}

TEST(VariableCrossType, IntVarLikeNonNumericStringDoesNotError) {
  // `like` asks the (int-typed) conversion node for a string, which passes
  // the literal straight through - so a pattern that is not a number is fine
  // here even though `=` against the same literal is an error.
  const cmp_outcome like = run_cmp(new_int_var(), op_like, factory::create_string("ab"), ctx_with(42, 0.0, ""));
  EXPECT_FALSE(like.truth);
  EXPECT_EQ("", like.eval_error);
}

TEST(VariableCrossType, IntVarLikeMatchesTextualRepresentation) {
  const cmp_outcome like = run_cmp(new_int_var(), op_like, factory::create_string("2"), ctx_with(42, 0.0, ""));
  EXPECT_TRUE(like.truth);
  EXPECT_EQ("", like.eval_error);
}

// ----------------------------------------------------------------------
// int/float mixing - no string ever enters the picture
// ----------------------------------------------------------------------

TEST(VariableCrossType, IntVarVsFloatLiteralPromotesVariableToFloat) {
  // The variable re-types itself to float rather than the literal being
  // squeezed into an int, so no precision is lost on either side.
  const node_type var = new_int_var();
  const cmp_outcome lt = run_cmp(var, op_lt, factory::create_float(3.5), ctx_with(3, 0.0, ""));
  EXPECT_TRUE(lt.truth);
  EXPECT_EQ("", lt.eval_error);
  EXPECT_EQ(type_float, var->get_type());
  // (int_variable_node::to_string() hardcodes the "{int}" tag, so the
  // rendered tree still says int even after the promotion.)
  EXPECT_EQ("{bool}({int}ivar < 3.5)", lt.tree);
}

TEST(VariableCrossType, IntVarVsFloatLiteralDoesNotRoundTheLiteral) {
  // 3 > 2.5 is true. Had the literal been converted into the int domain it
  // would have been rounded to 3 and this would be false - which is exactly
  // what happens for a summary variable, see
  // SummaryIntVarVsFloatLiteralRoundsTheLiteral below.
  const cmp_outcome gt = run_cmp(new_int_var(), op_gt, factory::create_float(2.5), ctx_with(3, 0.0, ""));
  EXPECT_TRUE(gt.truth);
  EXPECT_EQ("", gt.eval_error);
}

TEST(VariableCrossType, FloatVarVsIntLiteralComparesAsFloat) {
  const cmp_outcome gt = run_cmp(new_float_var(), op_gt, factory::create_int(2), ctx_with(0, 2.5, ""));
  EXPECT_TRUE(gt.truth);
  EXPECT_EQ("", gt.eval_error);
  const cmp_outcome eq = run_cmp(new_float_var(), op_eq, factory::create_int(2), ctx_with(0, 2.5, ""));
  EXPECT_FALSE(eq.truth);
}

// ----------------------------------------------------------------------
// string variable vs number literal - numbers win, the row value is parsed
// ----------------------------------------------------------------------

TEST(VariableCrossType, StringVarVsIntLiteralComparesAsNumbers) {
  // The variable joins the numeric domain and its row value is parsed, so
  // this is 10 = 10 (it used to be the rendered-text compare "10" = "10").
  const cmp_outcome eq = run_cmp(new_str_var(), op_eq, factory::create_int(10), ctx_with(0, 0.0, "10"));
  EXPECT_TRUE(eq.truth);
  EXPECT_EQ("{bool}({string}svar = 10)", eq.tree);
  const cmp_outcome ne = run_cmp(new_str_var(), op_eq, factory::create_int(10), ctx_with(0, 0.0, "11"));
  EXPECT_FALSE(ne.truth);
  EXPECT_EQ("", ne.eval_error);
}

TEST(VariableCrossType, StringVarOrderingAgainstIntLiteralIsNumeric) {
  // Numerically 10 > 9; the old lexical compare said "10" < "9". This is
  // the headline "numbers win" change.
  const cmp_outcome gt = run_cmp(new_str_var(), op_gt, factory::create_int(9), ctx_with(0, 0.0, "10"));
  EXPECT_TRUE(gt.truth);
  EXPECT_EQ("", gt.eval_error);
  const cmp_outcome lt = run_cmp(new_str_var(), op_lt, factory::create_int(9), ctx_with(0, 0.0, "10"));
  EXPECT_FALSE(lt.truth);
  EXPECT_EQ("", lt.eval_error);
}

TEST(VariableCrossType, StringVarVsFloatLiteralComparesNumerically) {
  const cmp_outcome eq = run_cmp(new_str_var(), op_eq, factory::create_float(2.5), ctx_with(0, 0.0, "2.5"));
  EXPECT_TRUE(eq.truth);
  EXPECT_EQ("", eq.eval_error);
  // Ordering is the discriminator: 10 > 2.5 as numbers, while the lexical
  // compare would say "10" < "2.5".
  const cmp_outcome gt = run_cmp(new_str_var(), op_gt, factory::create_float(2.5), ctx_with(0, 0.0, "10"));
  EXPECT_TRUE(gt.truth);
  EXPECT_EQ("", gt.eval_error);
}

TEST(VariableCrossType, StringVarNonNumericRowIsSureFalseAgainstNumbers) {
  // A row value that is not a number certainly does not satisfy a numeric
  // threshold: sure-false (no UNKNOWN escalation), for every operator
  // including '=' and '!=' - the no_value contract, matching optional
  // numbers with no value. The node warns (once) so the log explains why.
  const cmp_outcome gt = run_cmp(new_str_var(), op_gt, factory::create_int(9), ctx_with(0, 0.0, "abc"));
  EXPECT_FALSE(gt.truth);
  EXPECT_FALSE(gt.is_unsure);
  EXPECT_EQ("", gt.eval_error);
  const cmp_outcome eq = run_cmp(new_str_var(), op_eq, factory::create_int(9), ctx_with(0, 0.0, "abc"));
  EXPECT_FALSE(eq.truth);
  const cmp_outcome ne = run_cmp(new_str_var(), op_ne, factory::create_int(9), ctx_with(0, 0.0, "abc"));
  EXPECT_FALSE(ne.truth);
  EXPECT_FALSE(ne.is_unsure);
}

TEST(VariableCrossType, StringVarVsQuotedNumberStaysLexical) {
  // Quoting is how text ordering is asked for explicitly: both sides are
  // type_string, so "numbers win" never enters and "10" < "9" lexically.
  const cmp_outcome lt = run_cmp(new_str_var(), op_lt, factory::create_string("9"), ctx_with(0, 0.0, "10"));
  EXPECT_TRUE(lt.truth);
  EXPECT_EQ("", lt.eval_error);
}

// ----------------------------------------------------------------------
// Operand order does not matter: the string side joins the number domain
// from either side of the comparison.
// ----------------------------------------------------------------------

TEST(VariableCrossType, IntLiteralOnLeftOfStringVarComparesNumerically) {
  // `9 > svar` used to fail at evaluation (the variable ended up inside a
  // convert node that could not read it); it now mirrors `svar < 9`.
  const cmp_outcome gt = run_cmp(factory::create_int(9), op_gt, new_str_var(), ctx_with(0, 0.0, "10"));
  EXPECT_FALSE(gt.truth);
  EXPECT_EQ("", gt.infer_error);
  EXPECT_EQ("", gt.eval_error);
  const cmp_outcome gt2 = run_cmp(factory::create_int(11), op_gt, new_str_var(), ctx_with(0, 0.0, "10"));
  EXPECT_TRUE(gt2.truth);
  EXPECT_EQ("", gt2.eval_error);
}

TEST(VariableCrossType, StringLiteralOnLeftOfIntVarComparesNumerically) {
  // The quoted number joins the int variable's domain from the left too.
  const cmp_outcome lt = run_cmp(factory::create_string("9"), op_lt, new_int_var(), ctx_with(42, 0.0, ""));
  EXPECT_TRUE(lt.truth);
  EXPECT_EQ("", lt.infer_error);
  EXPECT_EQ("", lt.eval_error);
  const cmp_outcome lt2 = run_cmp(factory::create_string("9"), op_lt, new_int_var(), ctx_with(5, 0.0, ""));
  EXPECT_FALSE(lt2.truth);
  EXPECT_EQ("", lt2.eval_error);
}

TEST(VariableCrossType, FloatLiteralOnLeftOfIntVarIsFine) {
  // Numeric widening is done by re-inferring the variable, not by wrapping
  // it, so this direction keeps working.
  const cmp_outcome lt = run_cmp(factory::create_float(2.5), op_lt, new_int_var(), ctx_with(3, 0.0, ""));
  EXPECT_TRUE(lt.truth);
  EXPECT_EQ("", lt.eval_error);
}

// ----------------------------------------------------------------------
// dual variables adapt to the literal instead of falling back to text
// ----------------------------------------------------------------------

TEST(VariableCrossType, DualVarVsIntLiteralUsesIntAccessor) {
  const cmp_outcome gt = run_cmp(new_dual_int_str_var(), op_gt, factory::create_int(9), ctx_with(10, 0.0, "ten"));
  EXPECT_TRUE(gt.truth);
  EXPECT_EQ("", gt.eval_error);
  EXPECT_EQ("{bool}({int}dvar > 9)", gt.tree);
}

TEST(VariableCrossType, DualVarVsStringLiteralUsesStringAccessor) {
  const cmp_outcome eq = run_cmp(new_dual_int_str_var(), op_eq, factory::create_string("ten"), ctx_with(10, 0.0, "ten"));
  EXPECT_TRUE(eq.truth);
  EXPECT_EQ("", eq.eval_error);
  EXPECT_EQ("{bool}({string}dvar = \"ten\")", eq.tree);
}

TEST(VariableCrossType, DualVarVsNumericStringLiteralComparesAsStrings) {
  // A dual variable takes the *literal's* type, so a quoted number makes
  // this a text comparison: "10" < "9" lexically.
  const cmp_outcome lt = run_cmp(new_dual_int_str_var(), op_lt, factory::create_string("9"), ctx_with(10, 0.0, "10"));
  EXPECT_TRUE(lt.truth);
  EXPECT_EQ("", lt.eval_error);
}

TEST(VariableCrossType, DualIntStringVarVsFloatLiteralUsesIntAccessor) {
  // The float suggestion re-types the node to float; this dual node was
  // built with int+string accessors only, so the float request is served
  // from the int accessor (lossless). This is what lets a log or WMI
  // column be compared against a decimal threshold.
  const cmp_outcome gt = run_cmp(new_dual_int_str_var(), op_gt, factory::create_float(1.5), ctx_with(10, 0.0, "10"));
  EXPECT_TRUE(gt.truth);
  EXPECT_EQ("", gt.eval_error);
  const cmp_outcome gt2 = run_cmp(new_dual_int_str_var(), op_gt, factory::create_float(12.5), ctx_with(10, 0.0, "10"));
  EXPECT_FALSE(gt2.truth);
  EXPECT_EQ("", gt2.eval_error);
}

// ----------------------------------------------------------------------
// optional-int variables: numbers stay numbers, the string form is the
// presence test
// ----------------------------------------------------------------------

TEST(VariableCrossType, OptionalIntVarVsNoValueStringLiteral) {
  auto unset = make_var_context_with_object(without_opt());
  EXPECT_TRUE(run_cmp(make_opt_node(), op_eq, factory::create_string("unknown"), unset).truth);

  auto set = make_var_context_with_object(with_opt(42));
  EXPECT_FALSE(run_cmp(make_opt_node(), op_eq, factory::create_string("unknown"), set).truth);
}

TEST(VariableCrossType, OptionalIntVarVsNumericStringLiteralComparesRenderedValue) {
  auto set = make_var_context_with_object(with_opt(42));
  const cmp_outcome eq = run_cmp(make_opt_node(), op_eq, factory::create_string("42"), set);
  EXPECT_TRUE(eq.truth);
  EXPECT_EQ("", eq.eval_error);
}

TEST(VariableCrossType, OptionalIntVarWithNoValueIsSureFalseAgainstNumbers) {
  auto unset = make_var_context_with_object(without_opt());
  const cmp_outcome gt = run_cmp(make_opt_node(), op_gt, factory::create_int(5), unset);
  EXPECT_FALSE(gt.truth);
  EXPECT_FALSE(gt.is_unsure);
  EXPECT_EQ("", gt.eval_error);
}

// ----------------------------------------------------------------------
// summary variables ignore the type suggestion; a numeric-string literal
// still joins their int domain, and a float literal widens the comparison
// ----------------------------------------------------------------------

TEST(VariableCrossType, SummaryIntVarVsNumericStringComparesAsNumbers) {
  mock_summary summary;
  auto ctx = ctx_with_count(summary, 3);
  const cmp_outcome eq = run_cmp(new_summary_count_var(), op_eq, factory::create_string("3"), ctx);
  EXPECT_TRUE(eq.truth);
  EXPECT_EQ("", eq.eval_error);
}

TEST(VariableCrossType, SummaryIntVarVsFloatLiteralWidensToFloat) {
  // A summary variable keeps its int type, so the engine wraps it in a
  // convert-to-float node and answers the comparison in float. It used to be
  // the literal that was converted, rounding `count > 2.5` into `count > 3`.
  mock_summary summary;
  auto ctx = ctx_with_count(summary, 3);
  const cmp_outcome gt = run_cmp(new_summary_count_var(), op_gt, factory::create_float(2.5), ctx);
  EXPECT_TRUE(gt.truth);
  EXPECT_EQ("", gt.eval_error);
  EXPECT_EQ("{bool}({float}convert({int}count()) > 2.5)", gt.tree);

  mock_summary summary2;
  auto ctx2 = ctx_with_count(summary2, 2);
  const cmp_outcome gt2 = run_cmp(new_summary_count_var(), op_gt, factory::create_float(2.5), ctx2);
  EXPECT_FALSE(gt2.truth);
  EXPECT_EQ("", gt2.eval_error);
}

TEST(VariableCrossType, SummaryIntVarBinandFloatLiteralKeepsIntDomain) {
  // The float widening is gated to comparison operators: & has no float
  // form, so the literal still narrows into the int domain (2 & 3 = 2).
  mock_summary summary;
  auto ctx = ctx_with_count(summary, 2);
  const cmp_outcome band = run_cmp(new_summary_count_var(), op_binand, factory::create_float(2.5), ctx);
  EXPECT_TRUE(band.truth);
  EXPECT_EQ("", band.eval_error);
}
