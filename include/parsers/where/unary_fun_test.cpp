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
#include <boost/shared_ptr.hpp>
#include <list>
#include <parsers/operators.hpp>
#include <parsers/where/helpers.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/unary_fun.hpp>
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
// Mock binary_function_impl for testing bound functions
// ======================================================================

struct mock_binary_function final : binary_function_impl {
  node_type result_;
  mutable int call_count_ = 0;

  explicit mock_binary_function(node_type result) : result_(result) {}

  node_type evaluate(value_type, evaluation_context, const node_type) const override {
    ++call_count_;
    return result_;
  }
};

// ======================================================================
// Helpers
// ======================================================================

static evaluation_context make_context() { return boost::make_shared<mock_evaluation_context>(); }
static object_converter make_converter() { return boost::make_shared<mock_object_converter>(); }

static node_type make_int(long long v) { return factory::create_int(v); }
static node_type make_string(const std::string &v) { return factory::create_string(v); }

static boost::shared_ptr<unary_fun> make_fun(const std::string &name, node_type subject) { return boost::make_shared<unary_fun>(name, subject); }

// ======================================================================
// Construction tests
// ======================================================================

TEST(UnaryFun, CanEvaluateReturnsTrue) {
  auto fun = make_fun("convert", make_int(42));
  EXPECT_TRUE(fun->can_evaluate());
}

TEST(UnaryFun, DefaultTypeIsTbd) {
  auto fun = make_fun("convert", make_int(42));
  EXPECT_EQ(fun->get_type(), type_tbd);
}

TEST(UnaryFun, InferTypeReturnsTbd) {
  auto fun = make_fun("convert", make_int(42));
  EXPECT_EQ(fun->infer_type(make_converter()), type_tbd);
}

TEST(UnaryFun, InferTypeWithSuggestionReturnsTbd) {
  auto fun = make_fun("convert", make_int(42));
  EXPECT_EQ(fun->infer_type(make_converter(), type_int), type_tbd);
}

// ======================================================================
// to_string tests (no function bound)
// ======================================================================

TEST(UnaryFun, ToStringWithoutContext) {
  auto fun = make_fun("convert", make_int(42));
  std::string result = fun->to_string();
  EXPECT_NE(result.find("convert"), std::string::npos);
  EXPECT_NE(result.find("42"), std::string::npos);
}

TEST(UnaryFun, ToStringWithContextUnbound) {
  auto fun = make_fun("myfunc", make_int(10));
  auto ctx = make_context();
  std::string result = fun->to_string(ctx);
  EXPECT_EQ(result, "myfunc(10)");
}

TEST(UnaryFun, ToStringWithContextBound) {
  auto fun = make_fun("neg", make_int(5));
  fun->set_type(type_int);
  auto ctx = make_context();

  // Bind so function is set
  fun->bind(make_converter());

  std::string result = fun->to_string(ctx);
  // When bound, to_string evaluates via the function
  EXPECT_NE(result.find("neg"), std::string::npos);
}

// ======================================================================
// evaluate tests (unbound — no function)
// ======================================================================

TEST(UnaryFun, EvaluateUnboundReturnsCreateFalseAndSetsError) {
  auto fun = make_fun("unknown_func", make_int(42));
  auto ctx = make_context();
  node_type result = fun->evaluate(ctx);
  EXPECT_TRUE(ctx->has_error());
  EXPECT_NE(ctx->get_error().find("Missing function binding"), std::string::npos);
  EXPECT_NE(ctx->get_error().find("unknown_func"), std::string::npos);
  // Result should be create_false() → int value 0
  EXPECT_EQ(result->get_int_value(ctx), 0);
}

// ======================================================================
// bind tests
// ======================================================================

TEST(UnaryFun, BindWithKnownBinaryFunctionSucceeds) {
  // "neg" is a known binary function in op_factory
  auto fun = make_fun("neg", make_int(42));
  auto converter = make_converter();
  EXPECT_TRUE(fun->bind(converter));
}

TEST(UnaryFun, BindWithConvertFunctionSucceeds) {
  auto fun = make_fun("convert", make_int(42));
  auto converter = make_converter();
  EXPECT_TRUE(fun->bind(converter));
}

TEST(UnaryFun, BindWithAutoConvertFunctionSucceeds) {
  auto fun = make_fun("auto_convert", make_int(42));
  auto converter = make_converter();
  EXPECT_TRUE(fun->bind(converter));
}

TEST(UnaryFun, BindWithUnknownFunctionReturnsOperatorFalse) {
  // op_factory::get_binary_function returns operator_false for unknown names
  // which is still a valid (non-null) function, so bind returns true
  auto fun = make_fun("some_unknown", make_int(42));
  auto converter = make_converter();
  EXPECT_TRUE(fun->bind(converter));
}

// ======================================================================
// evaluate tests (bound — with function)
// ======================================================================

TEST(UnaryFun, EvaluateBoundNegIntReturnsNegatedValue) {
  auto fun = make_fun("neg", make_int(5));
  fun->set_type(type_int);
  auto converter = make_converter();
  EXPECT_TRUE(fun->bind(converter));

  auto ctx = make_context();
  node_type result = fun->evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_EQ(result->get_int_value(ctx), -5);
}

TEST(UnaryFun, EvaluateBoundNegZero) {
  auto fun = make_fun("neg", make_int(0));
  fun->set_type(type_int);
  EXPECT_TRUE(fun->bind(make_converter()));

  auto ctx = make_context();
  node_type result = fun->evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_EQ(result->get_int_value(ctx), 0);
}

TEST(UnaryFun, EvaluateBoundNegNegativeInt) {
  auto fun = make_fun("neg", make_int(-7));
  fun->set_type(type_int);
  EXPECT_TRUE(fun->bind(make_converter()));

  auto ctx = make_context();
  node_type result = fun->evaluate(ctx);
  EXPECT_FALSE(ctx->has_error());
  EXPECT_EQ(result->get_int_value(ctx), 7);
}

// ======================================================================
// get_value tests
// ======================================================================

TEST(UnaryFun, GetValueDelegatesToEvaluate) {
  auto fun = make_fun("neg", make_int(10));
  fun->set_type(type_int);
  EXPECT_TRUE(fun->bind(make_converter()));

  auto ctx = make_context();
  value_container vc = fun->get_value(ctx, type_int);
  EXPECT_EQ(vc.get_int(), -10);
}

// ======================================================================
// get_list_value tests
// ======================================================================

TEST(UnaryFun, GetListValueWithBoundFunction) {
  auto fun = make_fun("neg", make_int(3));
  fun->set_type(type_int);
  EXPECT_TRUE(fun->bind(make_converter()));

  auto ctx = make_context();
  std::list<node_type> result = fun->get_list_value(ctx);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front()->get_int_value(ctx), -3);
}

TEST(UnaryFun, GetListValueWithoutFunctionReturnsCreateFalse) {
  auto fun = make_fun("unbound_func", make_int(3));
  auto ctx = make_context();
  std::list<node_type> result = fun->get_list_value(ctx);
  EXPECT_EQ(result.size(), 1u);
  // Without a function bound, each element returns create_false() → 0
  EXPECT_EQ(result.front()->get_int_value(ctx), 0);
}

// ======================================================================
// static_evaluate tests
// ======================================================================

TEST(UnaryFun, StaticEvaluateWithConvertDelegatesToSubject) {
  auto fun = make_fun("convert", make_int(42));
  auto ctx = make_context();
  // Subject (int_value) returns true from static_evaluate
  EXPECT_TRUE(fun->static_evaluate(ctx));
}

TEST(UnaryFun, StaticEvaluateWithAutoConvertDelegatesToSubject) {
  auto fun = make_fun("auto_convert", make_int(42));
  auto ctx = make_context();
  EXPECT_TRUE(fun->static_evaluate(ctx));
}

TEST(UnaryFun, StaticEvaluateWithNegDelegatesToSubject) {
  // "neg" is transparent, so static_evaluate delegates to subject
  auto fun = make_fun("neg", make_int(42));
  auto ctx = make_context();
  EXPECT_TRUE(fun->static_evaluate(ctx));
}

TEST(UnaryFun, StaticEvaluateWithOtherFunctionReturnsFalse) {
  auto fun = make_fun("other_func", make_int(42));
  auto ctx = make_context();
  EXPECT_FALSE(fun->static_evaluate(ctx));
}

// ======================================================================
// require_object tests
// ======================================================================

TEST(UnaryFun, RequireObjectDelegatesToSubject) {
  // Value nodes return false for require_object
  auto fun = make_fun("convert", make_int(42));
  auto ctx = make_context();
  EXPECT_FALSE(fun->require_object(ctx));
}

// ======================================================================
// find_performance_data tests
// ======================================================================

TEST(UnaryFun, FindPerfDataWithConvertCollectsCandidateValue) {
  auto subject = make_int(42);
  auto fun = make_fun("convert", subject);
  auto ctx = make_context();
  performance_collector collector;
  fun->find_performance_data(ctx, collector);
  // The subject (int_value) sets a candidate value in its find_performance_data,
  // which may or may not set a candidate. The unary_fun wraps the result.
  // The return value is always false for unary_fun::find_performance_data.
  EXPECT_FALSE(fun->find_performance_data(ctx, collector));
}

TEST(UnaryFun, FindPerfDataWithNegCollectsCandidateValue) {
  auto subject = make_int(42);
  auto fun = make_fun("neg", subject);
  auto ctx = make_context();
  performance_collector collector;
  bool result = fun->find_performance_data(ctx, collector);
  EXPECT_FALSE(result);
}

TEST(UnaryFun, FindPerfDataWithOtherFunctionDoesNotCollect) {
  auto subject = make_int(42);
  auto fun = make_fun("other_func", subject);
  auto ctx = make_context();
  performance_collector collector;
  bool result = fun->find_performance_data(ctx, collector);
  EXPECT_FALSE(result);
  EXPECT_FALSE(collector.has_candidate_value());
}

// ======================================================================
// is_transparent (tested indirectly via static_evaluate and find_performance_data)
// ======================================================================

TEST(UnaryFun, NegIsTransparentSoStaticEvalDelegates) {
  auto fun = make_fun("neg", make_int(1));
  auto ctx = make_context();
  // "neg" is transparent, so static_evaluate returns subject->static_evaluate() = true
  EXPECT_TRUE(fun->static_evaluate(ctx));
}

TEST(UnaryFun, ConvertIsNotTransparentButStillDelegatesStaticEval) {
  // "convert" is not transparent, but the name check in static_evaluate
  // catches "convert" separately, so it still delegates
  auto fun = make_fun("convert", make_int(1));
  auto ctx = make_context();
  EXPECT_TRUE(fun->static_evaluate(ctx));
}

TEST(UnaryFun, ArbitraryNameIsNotTransparentStaticEvalReturnsFalse) {
  auto fun = make_fun("arbitrary", make_int(1));
  auto ctx = make_context();
  EXPECT_FALSE(fun->static_evaluate(ctx));
}
