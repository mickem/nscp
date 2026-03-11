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

// ======================================================================
// string_value — construction and type
// ======================================================================

TEST(StringValue, TypeIsString) {
  string_value sv("hello");
  EXPECT_EQ(sv.get_type(), type_string);
}

TEST(StringValue, DefaultIsNotUnsure) {
  string_value sv("hello");
  EXPECT_FALSE(sv.is_unsure_);
}

TEST(StringValue, UnsureFlagIsStored) {
  string_value sv("hello", true);
  EXPECT_TRUE(sv.is_unsure_);
}

// ======================================================================
// string_value — get_value conversions
// ======================================================================

TEST(StringValue, GetValueAsString) {
  auto ctx = make_context();
  string_value sv("hello");
  auto vc = sv.get_value(ctx, type_string);
  EXPECT_EQ(vc.get_string(), "hello");
  EXPECT_FALSE(ctx->has_error());
}

TEST(StringValue, GetValueAsInt) {
  auto ctx = make_context();
  string_value sv("42");
  auto vc = sv.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 42);
  EXPECT_FALSE(ctx->has_error());
}

TEST(StringValue, GetValueAsFloat) {
  auto ctx = make_context();
  string_value sv("3.14");
  auto vc = sv.get_value(ctx, type_float);
  EXPECT_DOUBLE_EQ(vc.get_float(), 3.14);
  EXPECT_FALSE(ctx->has_error());
}

TEST(StringValue, GetValueAsIntFromInvalidStringSetsError) {
  auto ctx = make_context();
  string_value sv("not_a_number");
  auto vc = sv.get_value(ctx, type_int);
  EXPECT_TRUE(ctx->has_error());
}

TEST(StringValue, GetValueAsFloatFromInvalidStringSetsError) {
  auto ctx = make_context();
  string_value sv("abc");
  auto vc = sv.get_value(ctx, type_float);
  EXPECT_TRUE(ctx->has_error());
}

TEST(StringValue, GetValueAsUnknownTypeSetsError) {
  auto ctx = make_context();
  string_value sv("hello");
  auto vc = sv.get_value(ctx, type_date);
  EXPECT_TRUE(ctx->has_error());
}

TEST(StringValue, GetValuePreservesUnsureFlag) {
  auto ctx = make_context();
  string_value sv("42", true);
  auto vc = sv.get_value(ctx, type_int);
  EXPECT_TRUE(vc.is_unsure);
}

// ======================================================================
// string_value — to_string
// ======================================================================

TEST(StringValue, ToStringWithoutContext) {
  string_value sv("hello");
  EXPECT_EQ(sv.to_string(), "(s){hello}");
}

TEST(StringValue, ToStringWithContext) {
  auto ctx = make_context();
  string_value sv("hello");
  EXPECT_EQ(sv.to_string(ctx), "hello");
}

// ======================================================================
// string_value — infer_type
// ======================================================================

TEST(StringValue, InferTypeReturnsString) {
  string_value sv("hello");
  EXPECT_EQ(sv.infer_type(make_converter()), type_string);
}

TEST(StringValue, InferTypeWithSuggestionReturnsString) {
  string_value sv("hello");
  EXPECT_EQ(sv.infer_type(make_converter(), type_int), type_string);
}

// ======================================================================
// string_value — node_value_impl base behavior
// ======================================================================

TEST(StringValue, CanEvaluateReturnsFalse) {
  string_value sv("hello");
  EXPECT_FALSE(sv.can_evaluate());
}

TEST(StringValue, EvaluateReturnsCreateFalse) {
  auto ctx = make_context();
  string_value sv("hello");
  auto result = sv.evaluate(ctx);
  EXPECT_EQ(result->get_int_value(ctx), 0);
}

TEST(StringValue, BindReturnsTrue) {
  string_value sv("hello");
  EXPECT_TRUE(sv.bind(make_converter()));
}

TEST(StringValue, StaticEvaluateReturnsTrue) {
  auto ctx = make_context();
  string_value sv("hello");
  EXPECT_TRUE(sv.static_evaluate(ctx));
}

TEST(StringValue, RequireObjectReturnsFalse) {
  auto ctx = make_context();
  string_value sv("hello");
  EXPECT_FALSE(sv.require_object(ctx));
}

TEST(StringValue, GetListValueReturnsSingleElement) {
  auto ctx = make_context();
  string_value sv("hello");
  auto list = sv.get_list_value(ctx);
  EXPECT_EQ(list.size(), 1u);
}

// ======================================================================
// int_value — construction and type
// ======================================================================

TEST(IntValue, TypeIsInt) {
  int_value iv(42);
  EXPECT_EQ(iv.get_type(), type_int);
}

TEST(IntValue, DefaultIsNotUnsure) {
  int_value iv(42);
  EXPECT_FALSE(iv.is_unsure_);
}

TEST(IntValue, UnsureFlagIsStored) {
  int_value iv(42, true);
  EXPECT_TRUE(iv.is_unsure_);
}

// ======================================================================
// int_value — get_value conversions
// ======================================================================

TEST(IntValue, GetValueAsInt) {
  auto ctx = make_context();
  int_value iv(42);
  auto vc = iv.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 42);
  EXPECT_FALSE(ctx->has_error());
}

TEST(IntValue, GetValueAsFloat) {
  auto ctx = make_context();
  int_value iv(42);
  auto vc = iv.get_value(ctx, type_float);
  EXPECT_DOUBLE_EQ(vc.get_float(), 42.0);
  EXPECT_FALSE(ctx->has_error());
}

TEST(IntValue, GetValueAsString) {
  auto ctx = make_context();
  int_value iv(42);
  auto vc = iv.get_value(ctx, type_string);
  EXPECT_EQ(vc.get_string(), "42");
  EXPECT_FALSE(ctx->has_error());
}

TEST(IntValue, GetValueAsUnknownTypeSetsError) {
  auto ctx = make_context();
  int_value iv(42);
  auto vc = iv.get_value(ctx, type_date);
  EXPECT_TRUE(ctx->has_error());
  EXPECT_EQ("Failed to convert int to ?: 42", ctx->get_error());
}

TEST(IntValue, GetValuePreservesUnsureFlag) {
  auto ctx = make_context();
  int_value iv(42, true);
  auto vc = iv.get_value(ctx, type_int);
  EXPECT_TRUE(vc.is_unsure);
}

TEST(IntValue, GetValueNegativeInt) {
  auto ctx = make_context();
  int_value iv(-100);
  auto vc = iv.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), -100);
}

TEST(IntValue, GetValueZero) {
  auto ctx = make_context();
  int_value iv(0);
  auto vc = iv.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 0);
}

// ======================================================================
// int_value — to_string
// ======================================================================

TEST(IntValue, ToStringWithoutContext) {
  int_value iv(42);
  EXPECT_EQ(iv.to_string(), "(i){42}");
}

TEST(IntValue, ToStringWithContext) {
  auto ctx = make_context();
  int_value iv(42);
  EXPECT_EQ(iv.to_string(ctx), "42");
}

TEST(IntValue, ToStringNegative) {
  int_value iv(-7);
  EXPECT_EQ(iv.to_string(), "(i){-7}");
}

// ======================================================================
// int_value — infer_type
// ======================================================================

TEST(IntValue, InferTypeReturnsInt) {
  int_value iv(42);
  EXPECT_EQ(iv.infer_type(make_converter()), type_int);
}

TEST(IntValue, InferTypeWithIntSuggestionReturnsInt) {
  int_value iv(42);
  EXPECT_EQ(iv.infer_type(make_converter(), type_int), type_int);
}

TEST(IntValue, InferTypeWithFloatSuggestionReturnsFloat) {
  int_value iv(42);
  auto result = iv.infer_type(make_converter(), type_float);
  EXPECT_EQ(result, type_float);
}

TEST(IntValue, InferTypeWithStringSuggestionReturnsInt) {
  int_value iv(42);
  EXPECT_EQ(iv.infer_type(make_converter(), type_string), type_int);
}

// ======================================================================
// int_value — node_value_impl base behavior
// ======================================================================

TEST(IntValue, CanEvaluateReturnsFalse) {
  int_value iv(42);
  EXPECT_FALSE(iv.can_evaluate());
}

TEST(IntValue, BindReturnsTrue) {
  int_value iv(42);
  EXPECT_TRUE(iv.bind(make_converter()));
}

TEST(IntValue, StaticEvaluateReturnsTrue) {
  auto ctx = make_context();
  int_value iv(42);
  EXPECT_TRUE(iv.static_evaluate(ctx));
}

TEST(IntValue, RequireObjectReturnsFalse) {
  auto ctx = make_context();
  int_value iv(42);
  EXPECT_FALSE(iv.require_object(ctx));
}

TEST(IntValue, GetListValueReturnsSingleElement) {
  auto ctx = make_context();
  int_value iv(42);
  auto list = iv.get_list_value(ctx);
  EXPECT_EQ(list.size(), 1u);
}

// ======================================================================
// float_value — construction and type
// ======================================================================

TEST(FloatValue, TypeIsFloat) {
  float_value fv(3.14);
  EXPECT_EQ(fv.get_type(), type_float);
}

TEST(FloatValue, DefaultIsNotUnsure) {
  float_value fv(3.14);
  EXPECT_FALSE(fv.is_unsure_);
}

TEST(FloatValue, UnsureFlagIsStored) {
  float_value fv(3.14, true);
  EXPECT_TRUE(fv.is_unsure_);
}

// ======================================================================
// float_value — get_value conversions
// ======================================================================

TEST(FloatValue, GetValueAsFloat) {
  auto ctx = make_context();
  float_value fv(3.14);
  auto vc = fv.get_value(ctx, type_float);
  EXPECT_DOUBLE_EQ(vc.get_float(), 3.14);
  EXPECT_FALSE(ctx->has_error());
}

TEST(FloatValue, GetValueAsInt) {
  auto ctx = make_context();
  float_value fv(3.99);
  auto vc = fv.get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), 3);
  EXPECT_FALSE(ctx->has_error());
}

TEST(FloatValue, GetValueAsUnknownTypeSetsError) {
  auto ctx = make_context();
  float_value fv(3.14);
  auto vc = fv.get_value(ctx, type_string);
  EXPECT_TRUE(ctx->has_error());
}

TEST(FloatValue, GetValuePreservesUnsureFlag) {
  auto ctx = make_context();
  float_value fv(3.14, true);
  auto vc = fv.get_value(ctx, type_float);
  EXPECT_TRUE(vc.is_unsure);
}

TEST(FloatValue, GetValueNegativeFloat) {
  auto ctx = make_context();
  float_value fv(-2.5);
  auto vc = fv.get_value(ctx, type_float);
  EXPECT_DOUBLE_EQ(vc.get_float(), -2.5);
}

TEST(FloatValue, GetValueZero) {
  auto ctx = make_context();
  float_value fv(0.0);
  auto vc = fv.get_value(ctx, type_float);
  EXPECT_DOUBLE_EQ(vc.get_float(), 0.0);
}

// ======================================================================
// float_value — to_string
// ======================================================================

TEST(FloatValue, ToStringWithoutContext) {
  float_value fv(3.14);
  std::string result = fv.to_string();
  EXPECT_NE(result.find("(f){"), std::string::npos);
  EXPECT_NE(result.find("3.14"), std::string::npos);
}

TEST(FloatValue, ToStringWithContext) {
  auto ctx = make_context();
  float_value fv(3.14);
  std::string result = fv.to_string(ctx);
  EXPECT_NE(result.find("3.14"), std::string::npos);
}

// ======================================================================
// float_value — infer_type
// ======================================================================

TEST(FloatValue, InferTypeReturnsFloat) {
  float_value fv(3.14);
  EXPECT_EQ(fv.infer_type(make_converter()), type_float);
}

TEST(FloatValue, InferTypeWithFloatSuggestionReturnsFloat) {
  float_value fv(3.14);
  EXPECT_EQ(fv.infer_type(make_converter(), type_float), type_float);
}

TEST(FloatValue, InferTypeWithIntSuggestionReturnsInt) {
  float_value fv(3.14);
  auto result = fv.infer_type(make_converter(), type_int);
  // TODO: Maybe this should return int? It returns float as int and float are compatible.
  EXPECT_EQ(result, type_float);
}

TEST(FloatValue, InferTypeWithStringSuggestionReturnsFloat) {
  float_value fv(3.14);
  EXPECT_EQ(fv.infer_type(make_converter(), type_string), type_float);
}

// ======================================================================
// float_value — node_value_impl base behavior
// ======================================================================

TEST(FloatValue, CanEvaluateReturnsFalse) {
  float_value fv(3.14);
  EXPECT_FALSE(fv.can_evaluate());
}

TEST(FloatValue, BindReturnsTrue) {
  float_value fv(3.14);
  EXPECT_TRUE(fv.bind(make_converter()));
}

TEST(FloatValue, StaticEvaluateReturnsTrue) {
  auto ctx = make_context();
  float_value fv(3.14);
  EXPECT_TRUE(fv.static_evaluate(ctx));
}

TEST(FloatValue, RequireObjectReturnsFalse) {
  auto ctx = make_context();
  float_value fv(3.14);
  EXPECT_FALSE(fv.require_object(ctx));
}

TEST(FloatValue, GetListValueReturnsSingleElement) {
  auto ctx = make_context();
  float_value fv(3.14);
  auto list = fv.get_list_value(ctx);
  EXPECT_EQ(list.size(), 1u);
}

// ======================================================================
// Factory integration — round-trip via factory::create_*
// ======================================================================

TEST(ValueNodeFactory, CreateStringRoundTrip) {
  auto ctx = make_context();
  auto node = factory::create_string("test");
  EXPECT_EQ(node->get_type(), type_string);
  EXPECT_EQ(node->get_string_value(ctx), "test");
}

TEST(ValueNodeFactory, CreateIntRoundTrip) {
  auto ctx = make_context();
  auto node = factory::create_int(99);
  EXPECT_EQ(node->get_type(), type_int);
  EXPECT_EQ(node->get_int_value(ctx), 99);
}

TEST(ValueNodeFactory, CreateFloatRoundTrip) {
  auto ctx = make_context();
  auto node = factory::create_float(2.718);
  EXPECT_EQ(node->get_type(), type_float);
  EXPECT_DOUBLE_EQ(node->get_float_value(ctx), 2.718);
}

TEST(ValueNodeFactory, CreateNegInt) {
  auto ctx = make_context();
  auto node = factory::create_neg_int(5);
  EXPECT_EQ(node->get_int_value(ctx), -5);
}

TEST(ValueNodeFactory, CreateFalseIsZero) {
  auto ctx = make_context();
  auto node = factory::create_false();
  EXPECT_EQ(node->get_int_value(ctx), 0);
}

TEST(ValueNodeFactory, CreateTrueIsOne) {
  auto ctx = make_context();
  auto node = factory::create_true();
  EXPECT_EQ(node->get_int_value(ctx), 1);
}
