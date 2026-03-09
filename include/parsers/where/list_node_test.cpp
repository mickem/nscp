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
#include <list>
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
  bool can_convert(std::string, boost::shared_ptr<any_node>, value_type) override { return false; }
  boost::shared_ptr<binary_function_impl> create_converter(std::string, boost::shared_ptr<any_node>, value_type) override { return nullptr; }
};

// ======================================================================
// Helpers
// ======================================================================

static evaluation_context make_context() { return boost::make_shared<mock_evaluation_context>(); }
static object_converter make_converter() { return boost::make_shared<mock_object_converter>(); }

// ======================================================================
// list_node — construction
// ======================================================================

TEST(ListNode, EmptyListToString) {
  const list_node node;
  EXPECT_EQ(node.to_string(), "");
}

TEST(ListNode, EmptyListToStringWithContext) {
  const list_node node;
  const auto ctx = make_context();
  EXPECT_EQ(node.to_string(ctx), "");
}

// ======================================================================
// list_node — to_string
// ======================================================================

TEST(ListNode, SingleElementToString) {
  list_node node;
  node.push_back(factory::create_string("hello"));
  EXPECT_EQ(node.to_string(), "(s){hello}");
}

TEST(ListNode, MultipleElementsToString) {
  list_node node;
  node.push_back(factory::create_string("a"));
  node.push_back(factory::create_string("b"));
  node.push_back(factory::create_string("c"));
  EXPECT_EQ(node.to_string(), "(s){a}, (s){b}, (s){c}");
}

TEST(ListNode, SingleElementToStringWithContext) {
  list_node node;
  node.push_back(factory::create_string("hello"));
  const auto ctx = make_context();
  EXPECT_EQ(node.to_string(ctx), "hello");
}

TEST(ListNode, MultipleElementsToStringWithContext) {
  list_node node;
  node.push_back(factory::create_string("a"));
  node.push_back(factory::create_string("b"));
  const auto ctx = make_context();
  EXPECT_EQ(node.to_string(ctx), "a, b");
}

TEST(ListNode, IntElementsToString) {
  list_node node;
  node.push_back(factory::create_int(42));
  node.push_back(factory::create_int(99));
  EXPECT_EQ(node.to_string(), "(i){42}, (i){99}");
}

TEST(ListNode, MixedElementsToString) {
  list_node node;
  node.push_back(factory::create_string("hello"));
  node.push_back(factory::create_int(42));
  EXPECT_EQ(node.to_string(), "(s){hello}, (i){42}");
}

// ======================================================================
// list_node — get_value
// ======================================================================

TEST(ListNode, GetValueAsString) {
  list_node node;
  node.push_back(factory::create_string("x"));
  node.push_back(factory::create_string("y"));
  const auto ctx = make_context();
  const auto val = node.get_value(ctx, type_string);
  EXPECT_TRUE(val.is(type_string));
  EXPECT_EQ(val.get_string(""), "x, y");
}

TEST(ListNode, GetValueAsStringSingleElement) {
  list_node node;
  node.push_back(factory::create_string("only"));
  const auto ctx = make_context();
  const auto val = node.get_value(ctx, type_string);
  EXPECT_TRUE(val.is(type_string));
  EXPECT_EQ(val.get_string(""), "only");
}

TEST(ListNode, GetValueAsStringEmpty) {
  const list_node node;
  const auto ctx = make_context();
  const auto val = node.get_value(ctx, type_string);
  EXPECT_TRUE(val.is(type_string));
  EXPECT_EQ(val.get_string(""), "");
}

TEST(ListNode, GetValueAsIntReturnsNilAndSetsError) {
  list_node node;
  node.push_back(factory::create_int(1));
  const auto ctx = make_context();
  auto val = node.get_value(ctx, type_int);
  EXPECT_TRUE(ctx->has_error());
}

TEST(ListNode, GetValueAsFloatReturnsNilAndSetsError) {
  list_node node;
  node.push_back(factory::create_float(1.5));
  const auto ctx = make_context();
  auto val = node.get_value(ctx, type_float);
  EXPECT_TRUE(ctx->has_error());
}

// ======================================================================
// list_node — get_list_value
// ======================================================================

TEST(ListNode, GetListValueReturnsAllNodes) {
  list_node node;
  node.push_back(factory::create_string("a"));
  node.push_back(factory::create_string("b"));
  node.push_back(factory::create_string("c"));
  const auto ctx = make_context();
  const auto result = node.get_list_value(ctx);
  EXPECT_EQ(result.size(), 3u);
}

TEST(ListNode, GetListValueEmptyList) {
  list_node node;
  const auto ctx = make_context();
  const auto result = node.get_list_value(ctx);
  EXPECT_TRUE(result.empty());
}

// ======================================================================
// list_node — can_evaluate
// ======================================================================

TEST(ListNode, CanEvaluateReturnsFalse) {
  const list_node node;
  EXPECT_FALSE(node.can_evaluate());
}

// ======================================================================
// list_node — evaluate
// ======================================================================

TEST(ListNode, EvaluateReturnsNode) {
  list_node node;
  node.push_back(factory::create_string("a"));
  const auto ctx = make_context();
  const auto result = node.evaluate(ctx);
  ASSERT_TRUE(result);
}

TEST(ListNode, EvaluateEmptyListReturnsNode) {
  const list_node node;
  const auto ctx = make_context();
  const auto result = node.evaluate(ctx);
  ASSERT_TRUE(result);
}

// ======================================================================
// list_node — bind
// ======================================================================

TEST(ListNode, BindEmptyListReturnsTrue) {
  list_node node;
  const auto conv = make_converter();
  EXPECT_TRUE(node.bind(conv));
}

TEST(ListNode, BindWithStringNodesReturnsTrue) {
  list_node node;
  node.push_back(factory::create_string("a"));
  node.push_back(factory::create_string("b"));
  const auto conv = make_converter();
  EXPECT_TRUE(node.bind(conv));
}

// ======================================================================
// list_node — infer_type (with suggestion)
// ======================================================================

TEST(ListNode, InferTypeWithSuggestionReturnsTbd) {
  list_node node;
  node.push_back(factory::create_string("a"));
  const auto conv = make_converter();
  const auto result = node.infer_type(conv, type_string);
  EXPECT_EQ(result, type_tbd);
}

// ======================================================================
// list_node — infer_type (without suggestion)
// ======================================================================

TEST(ListNode, InferTypeEmptyListReturnsTbd) {
  list_node node;
  const auto conv = make_converter();
  const auto result = node.infer_type(conv);
  EXPECT_EQ(result, type_tbd);
}

TEST(ListNode, InferTypeHomogeneousStrings) {
  list_node node;
  node.push_back(factory::create_string("a"));
  node.push_back(factory::create_string("b"));
  const auto conv = make_converter();
  const auto result = node.infer_type(conv);
  EXPECT_EQ(result, type_string);
}

TEST(ListNode, InferTypeHomogeneousInts) {
  list_node node;
  node.push_back(factory::create_int(1));
  node.push_back(factory::create_int(2));
  const auto conv = make_converter();
  const auto result = node.infer_type(conv);
  EXPECT_EQ(result, type_int);
}

TEST(ListNode, InferTypeMixedReturnsTbd) {
  list_node node;
  node.push_back(factory::create_string("a"));
  node.push_back(factory::create_int(1));
  const auto conv = make_converter();
  const auto result = node.infer_type(conv);
  EXPECT_EQ(result, type_tbd);
}

TEST(ListNode, InferTypeSingleElement) {
  list_node node;
  node.push_back(factory::create_int(42));
  const auto conv = make_converter();
  const auto result = node.infer_type(conv);
  EXPECT_EQ(result, type_int);
}

// ======================================================================
// list_node — static_evaluate
// ======================================================================

TEST(ListNode, StaticEvaluateReturnsTrue) {
  list_node node;
  node.push_back(factory::create_string("a"));
  const auto ctx = make_context();
  EXPECT_TRUE(node.static_evaluate(ctx));
}

TEST(ListNode, StaticEvaluateEmptyReturnsTrue) {
  list_node node;
  const auto ctx = make_context();
  EXPECT_TRUE(node.static_evaluate(ctx));
}

// ======================================================================
// list_node — require_object
// ======================================================================

TEST(ListNode, RequireObjectReturnsTrue) {
  list_node node;
  node.push_back(factory::create_string("a"));
  const auto ctx = make_context();
  EXPECT_TRUE(node.require_object(ctx));
}

TEST(ListNode, RequireObjectEmptyReturnsTrue) {
  const list_node node;
  const auto ctx = make_context();
  EXPECT_TRUE(node.require_object(ctx));
}

// ======================================================================
// list_node — find_performance_data
// ======================================================================

TEST(ListNode, FindPerformanceDataReturnsFalse) {
  list_node node;
  node.push_back(factory::create_string("a"));
  const auto ctx = make_context();
  performance_collector collector;
  EXPECT_FALSE(node.find_performance_data(ctx, collector));
}

// ======================================================================
// factory — create_list
// ======================================================================

TEST(ListNode, FactoryCreateListFromStrings) {
  const std::list<std::string> items = {"alpha", "beta", "gamma"};
  const auto node = factory::create_list(items);
  ASSERT_TRUE(node);
  EXPECT_EQ(node->to_string(), "(s){alpha}, (s){beta}, (s){gamma}");
}

TEST(ListNode, FactoryCreateListFromInts) {
  const std::list<long long> items = {1, 2, 3};
  const auto node = factory::create_list(items);
  ASSERT_TRUE(node);
  EXPECT_EQ(node->to_string(), "(i){1}, (i){2}, (i){3}");
}

TEST(ListNode, FactoryCreateListFromDoubles) {
  const std::list<double> items = {1.5, 2.5};
  const auto node = factory::create_list(items);
  ASSERT_TRUE(node);
  // Float values have their own to_string format
  const auto str = node->to_string();
  EXPECT_FALSE(str.empty());
}

TEST(ListNode, FactoryCreateEmptyList) {
  const auto node = factory::create_list();
  ASSERT_TRUE(node);
  EXPECT_EQ(node->to_string(), "");
}

TEST(ListNode, FactoryCreateListPushBack) {
  const auto node = factory::create_list();
  node->push_back(factory::create_string("one"));
  node->push_back(factory::create_string("two"));
  EXPECT_EQ(node->to_string(), "(s){one}, (s){two}");
}

TEST(ListNode, FactoryCreateListFromEmptyStringList) {
  const std::list<std::string> items;
  const auto node = factory::create_list(items);
  ASSERT_TRUE(node);
  EXPECT_EQ(node->to_string(), "");
}

TEST(ListNode, FactoryCreateListFromEmptyIntList) {
  const std::list<long long> items;
  const auto node = factory::create_list(items);
  ASSERT_TRUE(node);
  EXPECT_EQ(node->to_string(), "");
}
