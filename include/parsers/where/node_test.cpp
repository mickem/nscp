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
// Mock object_factory (extends object_converter with variable/function support)
// ======================================================================

struct mock_object_factory final : object_factory_interface {
  std::string error_;
  std::string warn_;
  bool debug_enabled_ = false;
  std::map<std::string, node_type> variables_;

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
// value_container tests
// ======================================================================

TEST(ValueContainer, CreateInt) {
  const auto vc = value_container::create_int(42);
  EXPECT_EQ(vc.get_int(), 42);
  EXPECT_EQ(vc.get_string(), "42");
  EXPECT_FALSE(vc.is_unsure);
}

TEST(ValueContainer, CreateIntUnsure) {
  const auto vc = value_container::create_int(7, true);
  EXPECT_EQ(vc.get_int(), 7);
  EXPECT_TRUE(vc.is_unsure);
}

TEST(ValueContainer, CreateFloat) {
  const auto vc = value_container::create_float(3.14);
  EXPECT_DOUBLE_EQ(vc.get_float(), 3.14);
  EXPECT_FALSE(vc.is_unsure);
}

TEST(ValueContainer, CreateString) {
  const auto vc = value_container::create_string("hello");
  EXPECT_EQ(vc.get_string(), "hello");
  EXPECT_FALSE(vc.is_unsure);
}

TEST(ValueContainer, CreateBoolTrue) {
  const auto vc = value_container::create_bool(true);
  EXPECT_EQ(vc.get_int(), 1);
  EXPECT_TRUE(vc.is_true());
}

TEST(ValueContainer, CreateBoolFalse) {
  const auto vc = value_container::create_bool(false);
  EXPECT_EQ(vc.get_int(), 0);
  EXPECT_FALSE(vc.is_true());
}

TEST(ValueContainer, CreateNil) {
  const auto vc = value_container::create_nil();
  EXPECT_THROW(vc.get_int(), filter_exception);
  EXPECT_THROW(vc.get_float(), filter_exception);
  EXPECT_THROW(vc.get_string(), filter_exception);
}

TEST(ValueContainer, GetStringDefault) {
  const auto vc = value_container::create_nil();
  EXPECT_EQ(vc.get_string("default"), "default");
}

TEST(ValueContainer, GetIntDefault) {
  const auto vc = value_container::create_nil();
  EXPECT_EQ(vc.get_int(99), 99);
}

TEST(ValueContainer, GetFloatDefault) {
  const auto vc = value_container::create_nil();
  EXPECT_DOUBLE_EQ(vc.get_float(1.5), 1.5);
}

TEST(ValueContainer, GetStringFromInt) {
  const auto vc = value_container::create_int(123);
  EXPECT_EQ(vc.get_string(), "123");
  EXPECT_EQ(vc.get_string("unused"), "123");
}

TEST(ValueContainer, GetStringFromFloat) {
  const auto vc = value_container::create_float(2.5);
  std::string s = vc.get_string();
  EXPECT_FALSE(s.empty());
}

TEST(ValueContainer, GetIntFromFloat) {
  const auto vc = value_container::create_float(9.7);
  EXPECT_EQ(vc.get_int(), 9);
}

TEST(ValueContainer, GetFloatFromInt) {
  const auto vc = value_container::create_int(5);
  EXPECT_DOUBLE_EQ(vc.get_float(), 5.0);
}

TEST(ValueContainer, IsTypeInt) {
  const auto vc = value_container::create_int(1);
  EXPECT_TRUE(vc.is(type_int));
  EXPECT_FALSE(vc.is(type_float));
  EXPECT_FALSE(vc.is(type_string));
}

TEST(ValueContainer, IsTypeFloat) {
  const auto vc = value_container::create_float(1.0);
  EXPECT_FALSE(vc.is(type_int));
  EXPECT_TRUE(vc.is(type_float));
  EXPECT_FALSE(vc.is(type_string));
}

TEST(ValueContainer, IsTypeString) {
  const auto vc = value_container::create_string("x");
  EXPECT_FALSE(vc.is(type_int));
  EXPECT_FALSE(vc.is(type_float));
  EXPECT_TRUE(vc.is(type_string));
}

TEST(ValueContainer, CopyConstructor) {
  const auto vc1 = value_container::create_int(42);
  const value_container vc2(vc1);
  EXPECT_EQ(vc2.get_int(), 42);
  EXPECT_EQ(vc2.is_unsure, vc1.is_unsure);
}

TEST(ValueContainer, AssignmentOperator) {
  const auto vc1 = value_container::create_string("abc");
  value_container vc2;
  vc2 = vc1;
  EXPECT_EQ(vc2.get_string(), "abc");
}

// ======================================================================
// filter_exception tests
// ======================================================================

TEST(FilterException, WhatReturnsMessage) {
  const filter_exception ex("something went wrong");
  EXPECT_STREQ(ex.what(), "something went wrong");
}

TEST(FilterException, ReasonReturnsString) {
  const filter_exception ex("test error");
  const std::string reason = ex.reason();
  EXPECT_FALSE(reason.empty());
}

// ======================================================================
// any_node type query tests (via value nodes)
// ======================================================================

TEST(AnyNode, IntNodeIsInt) {
  const auto node = factory::create_int(10);
  EXPECT_TRUE(node->is_int());
  EXPECT_TRUE(node->is_float());  // Int can be converted losslessly to a float
  EXPECT_FALSE(node->is_string());
  EXPECT_EQ(node->get_type(), type_int);
}

TEST(AnyNode, FloatNodeIsFloat) {
  const auto node = factory::create_float(1.5);
  EXPECT_FALSE(node->is_int());
  EXPECT_TRUE(node->is_float());
  EXPECT_FALSE(node->is_string());
  EXPECT_EQ(node->get_type(), type_float);
}

TEST(AnyNode, StringNodeIsString) {
  const auto node = factory::create_string("abc");
  EXPECT_FALSE(node->is_int());
  EXPECT_FALSE(node->is_float());
  EXPECT_TRUE(node->is_string());
  EXPECT_EQ(node->get_type(), type_string);
}

TEST(AnyNode, GetIntValue) {
  const auto node = factory::create_int(42);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 42);
}

TEST(AnyNode, GetFloatValue) {
  const auto node = factory::create_float(2.5);
  const auto ctx = make_context();
  EXPECT_DOUBLE_EQ(node->get_float_value(ctx), 2.5);
}

TEST(AnyNode, GetStringValue) {
  const auto node = factory::create_string("hello");
  const auto ctx = make_context();
  EXPECT_EQ(node->get_string_value(ctx), "hello");
}

TEST(AnyNode, SetType) {
  const auto node = factory::create_int(1);
  EXPECT_EQ(node->get_type(), type_int);
  node->set_type(type_float);
  EXPECT_EQ(node->get_type(), type_float);
}

// ======================================================================
// performance_collector tests
// ======================================================================

TEST(PerformanceCollector, InitiallyNoCandidates) {
  const performance_collector pc;
  EXPECT_FALSE(pc.has_candidates());
  EXPECT_FALSE(pc.has_candidate_value());
  EXPECT_FALSE(pc.has_candidate_variable());
}

TEST(PerformanceCollector, SetCandidateVariable) {
  performance_collector pc;
  pc.set_candidate_variable("cpu_load");
  EXPECT_TRUE(pc.has_candidate_variable());
  EXPECT_TRUE(pc.has_candidates());
  EXPECT_EQ(pc.get_variable(), "cpu_load");
}

TEST(PerformanceCollector, SetCandidateValue) {
  performance_collector pc;
  const auto val = factory::create_int(100);
  pc.set_candidate_value(val);
  EXPECT_TRUE(pc.has_candidate_value());
  EXPECT_TRUE(pc.has_candidates());
  EXPECT_EQ(pc.get_value(), val);
}

TEST(PerformanceCollector, AddPerfMergesBoundries) {
  performance_collector pc1;
  performance_collector pc2;

  pc2.set_candidate_variable("var1");
  pc2.set_candidate_value(factory::create_int(50));

  // Add bounds to pc2 first
  performance_collector left;
  left.set_candidate_variable("var1");
  performance_collector right;
  right.set_candidate_value(factory::create_int(100));
  pc2.add_bounds_candidates(left, right);

  pc1.add_perf(pc2);
  auto candidates = pc1.get_candidates();
  EXPECT_EQ(candidates.size(), 1u);
  EXPECT_TRUE(candidates.find("var1") != candidates.end());
}

TEST(PerformanceCollector, AddBoundsCandidatesUpper) {
  performance_collector pc;
  performance_collector lower;
  lower.set_candidate_variable("mem");
  performance_collector upper;
  upper.set_candidate_value(factory::create_int(1024));

  EXPECT_TRUE(pc.add_bounds_candidates(lower, upper));
  auto candidates = pc.get_candidates();
  EXPECT_EQ(candidates.size(), 1u);
  auto it = candidates.find("mem");
  ASSERT_NE(it, candidates.end());
  EXPECT_EQ(it->second.perf_node_type, performance_node::perf_type_upper);
}

TEST(PerformanceCollector, AddBoundsCandidatesLower) {
  performance_collector pc;
  performance_collector lower;
  lower.set_candidate_value(factory::create_int(0));
  performance_collector upper;
  upper.set_candidate_variable("disk");

  EXPECT_TRUE(pc.add_bounds_candidates(lower, upper));
  auto candidates = pc.get_candidates();
  EXPECT_EQ(candidates.size(), 1u);
  auto it = candidates.find("disk");
  ASSERT_NE(it, candidates.end());
  EXPECT_EQ(it->second.perf_node_type, performance_node::perf_type_lower);
}

TEST(PerformanceCollector, AddBoundsCandidatesNeitherHasVariable) {
  performance_collector pc;
  performance_collector left;
  left.set_candidate_value(factory::create_int(1));
  performance_collector right;
  right.set_candidate_value(factory::create_int(2));

  EXPECT_FALSE(pc.add_bounds_candidates(left, right));
  EXPECT_EQ(pc.get_candidates().size(), 0u);
}

TEST(PerformanceCollector, AddBoundsCandidatesDoesNotOverwrite) {
  performance_collector pc;

  // First add
  performance_collector left1;
  left1.set_candidate_variable("x");
  performance_collector right1;
  right1.set_candidate_value(factory::create_int(10));
  EXPECT_TRUE(pc.add_bounds_candidates(left1, right1));

  // Second add with same variable — should not overwrite
  performance_collector left2;
  left2.set_candidate_variable("x");
  performance_collector right2;
  right2.set_candidate_value(factory::create_int(20));
  EXPECT_TRUE(pc.add_bounds_candidates(left2, right2));

  auto candidates = pc.get_candidates();
  EXPECT_EQ(candidates.size(), 1u);
  auto ctx = make_context();
  EXPECT_EQ(candidates["x"].value->get_int_value(ctx), 10);
}

TEST(PerformanceCollector, AddNeutralCandidatesLeftVariable) {
  performance_collector pc;
  performance_collector left;
  left.set_candidate_variable("cpu");
  performance_collector right;
  right.set_candidate_value(factory::create_int(50));

  EXPECT_TRUE(pc.add_neutral_candidates(left, right));
  auto candidates = pc.get_candidates();
  EXPECT_EQ(candidates.size(), 1u);
  auto it = candidates.find("cpu");
  ASSERT_NE(it, candidates.end());
  EXPECT_EQ(it->second.perf_node_type, performance_node::perf_type_neutral);
}

TEST(PerformanceCollector, AddNeutralCandidatesRightVariable) {
  performance_collector pc;
  performance_collector left;
  left.set_candidate_value(factory::create_int(50));
  performance_collector right;
  right.set_candidate_variable("net");

  EXPECT_TRUE(pc.add_neutral_candidates(left, right));
  auto candidates = pc.get_candidates();
  EXPECT_EQ(candidates.size(), 1u);
  auto it = candidates.find("net");
  ASSERT_NE(it, candidates.end());
  EXPECT_EQ(it->second.perf_node_type, performance_node::perf_type_neutral);
}

TEST(PerformanceCollector, AddNeutralCandidatesNeitherHasVariable) {
  performance_collector pc;
  performance_collector left;
  left.set_candidate_value(factory::create_int(1));
  performance_collector right;
  right.set_candidate_value(factory::create_int(2));

  EXPECT_FALSE(pc.add_neutral_candidates(left, right));
}

TEST(PerformanceCollector, AddNeutralCandidatesDoesNotOverwrite) {
  performance_collector pc;

  performance_collector left1;
  left1.set_candidate_variable("y");
  performance_collector right1;
  right1.set_candidate_value(factory::create_int(100));
  EXPECT_TRUE(pc.add_neutral_candidates(left1, right1));

  performance_collector left2;
  left2.set_candidate_variable("y");
  performance_collector right2;
  right2.set_candidate_value(factory::create_int(200));
  EXPECT_TRUE(pc.add_neutral_candidates(left2, right2));

  auto candidates = pc.get_candidates();
  EXPECT_EQ(candidates.size(), 1u);
  auto ctx = make_context();
  EXPECT_EQ(candidates["y"].value->get_int_value(ctx), 100);
}

// ======================================================================
// factory tests
// ======================================================================

TEST(Factory, CreateInt) {
  const auto node = factory::create_int(42);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 42);
  EXPECT_EQ(node->get_type(), type_int);
}

TEST(Factory, CreateFloat) {
  const auto node = factory::create_float(3.14);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_DOUBLE_EQ(node->get_float_value(ctx), 3.14);
  EXPECT_EQ(node->get_type(), type_float);
}

TEST(Factory, CreateString) {
  const auto node = factory::create_string("test");
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_string_value(ctx), "test");
  EXPECT_EQ(node->get_type(), type_string);
}

TEST(Factory, CreateNegInt) {
  const auto node = factory::create_neg_int(5);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), -5);
}

TEST(Factory, CreateFalse) {
  const auto node = factory::create_false();
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 0);
}

TEST(Factory, CreateTrue) {
  const auto node = factory::create_true();
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 1);
}

TEST(Factory, CreateIosInt) {
  const auto node = factory::create_ios(static_cast<long long>(7));
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 7);
}

TEST(Factory, CreateIosFloat) {
  const auto node = factory::create_ios(2.5);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_DOUBLE_EQ(node->get_float_value(ctx), 2.5);
}

TEST(Factory, CreateIosString) {
  const auto node = factory::create_ios(std::string("abc"));
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_string_value(ctx), "abc");
}

TEST(Factory, CreateListFromStrings) {
  std::list<std::string> values = {"a", "b", "c"};
  const auto node = factory::create_list(values);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  const auto list = node->get_list_value(ctx);
  EXPECT_EQ(list.size(), 3u);
}

TEST(Factory, CreateListFromInts) {
  const std::list<long long> values = {1, 2, 3};
  const auto node = factory::create_list(values);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  const auto list = node->get_list_value(ctx);
  EXPECT_EQ(list.size(), 3u);
}

TEST(Factory, CreateListFromDoubles) {
  const std::list<double> values = {1.1, 2.2};
  const auto node = factory::create_list(values);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  const auto list = node->get_list_value(ctx);
  EXPECT_EQ(list.size(), 2u);
}

TEST(Factory, CreateEmptyList) {
  const auto node = factory::create_list();
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  const auto list = node->get_list_value(ctx);
  EXPECT_EQ(list.size(), 0u);
}

TEST(Factory, CreateNumFromIntContainer) {
  const auto vc = value_container::create_int(99);
  const auto node = factory::create_num(vc);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 99);
}

TEST(Factory, CreateNumFromFloatContainer) {
  const auto vc = value_container::create_float(1.5);
  const auto node = factory::create_num(vc);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_DOUBLE_EQ(node->get_float_value(ctx), 1.5);
}

TEST(Factory, CreateNumFromStringContainer) {
  const auto vc = value_container::create_string("hello");
  const auto node = factory::create_num(vc);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_string_value(ctx), "hello");
}

TEST(Factory, CreateNumFromNilContainerReturnsZero) {
  const auto vc = value_container::create_nil();
  const auto node = factory::create_num(vc);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 0);
}

TEST(Factory, CreateBinOp) {
  const auto lhs = factory::create_int(10);
  const auto rhs = factory::create_int(20);
  const auto node = factory::create_bin_op(op_eq, lhs, rhs);
  ASSERT_NE(node, nullptr);
}

TEST(Factory, CreateUnOp) {
  const auto operand = factory::create_int(1);
  const auto node = factory::create_un_op(op_not, operand);
  ASSERT_NE(node, nullptr);
}

TEST(Factory, CreateConversion) {
  const auto operand = factory::create_int(42);
  const auto node = factory::create_conversion(operand);
  ASSERT_NE(node, nullptr);
}

TEST(Factory, CreateVariableKnown) {
  const auto val = factory::create_int(42);
  const auto f = make_factory_with_variable("test_var", val);
  const auto node = factory::create_variable(f, "test_var");
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 42);
}

TEST(Factory, CreateVariableUnknownReturnsFalse) {
  const auto f = make_factory();
  const auto node = factory::create_variable(f, "unknown_var");
  ASSERT_NE(node, nullptr);
  // Should return false (0) for unknown variables
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 0);
  // Factory should have received an error
  EXPECT_TRUE(f->has_error());
}

TEST(Factory, CreateFunUnknownFunctionReturnsFalse) {
  const auto f = make_factory();
  const auto operand = factory::create_int(1);
  const auto node = factory::create_fun(f, "nonexistent_func", operand);
  ASSERT_NE(node, nullptr);
  const auto ctx = make_context();
  EXPECT_EQ(node->get_int_value(ctx), 0);
  EXPECT_TRUE(f->has_error());
}
