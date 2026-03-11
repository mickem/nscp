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

#include <parsers/where/helpers.hpp>
#include <parsers/where/value_node.hpp>
#include <string>

#include <parsers/where/variable.hpp>

using namespace parsers::where;
using namespace parsers::where::helpers;

// ======================================================================
// Mock evaluation context
// ======================================================================

struct mock_eval_context final : evaluation_context_interface {
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

struct mock_converter final : object_converter_interface {
  std::string error_;
  std::string warn_;
  bool debug_enabled_ = false;
  bool can_convert_result_ = false;

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

  bool can_convert(value_type, value_type) override { return can_convert_result_; }
  bool can_convert(std::string, std::shared_ptr<any_node>, value_type) override { return false; }
  std::shared_ptr<binary_function_impl> create_converter(std::string, std::shared_ptr<any_node>, value_type) override { return nullptr; }
};

static evaluation_context make_eval_context() { return std::make_shared<mock_eval_context>(); }
static object_converter make_converter(bool can_convert_result = false) {
  auto c = std::make_shared<mock_converter>();
  c->can_convert_result_ = can_convert_result;
  return c;
}

// ======================================================================
// type_to_string
// ======================================================================

TEST(WhereHelpers, TypeToStringBool) { EXPECT_EQ("bool", type_to_string(type_bool)); }

TEST(WhereHelpers, TypeToStringString) { EXPECT_EQ("string", type_to_string(type_string)); }

TEST(WhereHelpers, TypeToStringInt) { EXPECT_EQ("int", type_to_string(type_int)); }

TEST(WhereHelpers, TypeToStringFloat) { EXPECT_EQ("float", type_to_string(type_float)); }

TEST(WhereHelpers, TypeToStringDate) { EXPECT_EQ("date", type_to_string(type_date)); }

TEST(WhereHelpers, TypeToStringSize) { EXPECT_EQ("size", type_to_string(type_size)); }

TEST(WhereHelpers, TypeToStringInvalid) { EXPECT_EQ("invalid", type_to_string(type_invalid)); }

TEST(WhereHelpers, TypeToStringTbd) { EXPECT_EQ("tbd", type_to_string(type_tbd)); }

TEST(WhereHelpers, TypeToStringCustom) {
  EXPECT_EQ("u:0", type_to_string(type_custom));
  EXPECT_EQ("u:1", type_to_string(type_custom_1));
}

TEST(WhereHelpers, TypeToStringCustomFloat) {
  EXPECT_EQ("uf:0", type_to_string(type_custom_float));
  EXPECT_EQ("uf:1", type_to_string(type_custom_float_1));
}

TEST(WhereHelpers, TypeToStringCustomString) {
  EXPECT_EQ("us:0", type_to_string(type_custom_string));
  EXPECT_EQ("us:1", type_to_string(type_custom_string_1));
}

TEST(WhereHelpers, TypeToStringCustomInt) {
  EXPECT_EQ("ui:0", type_to_string(type_custom_int));
  EXPECT_EQ("ui:1", type_to_string(type_custom_int_1));
}

// ======================================================================
// type_is_int
// ======================================================================

TEST(WhereHelpers, TypeIsIntForIntTypes) {
  EXPECT_TRUE(type_is_int(type_int));
  EXPECT_TRUE(type_is_int(type_bool));
  EXPECT_TRUE(type_is_int(type_date));
  EXPECT_TRUE(type_is_int(type_size));
}

TEST(WhereHelpers, TypeIsIntForCustomInt) {
  EXPECT_TRUE(type_is_int(type_custom_int));
  EXPECT_TRUE(type_is_int(type_custom_int_1));
  EXPECT_TRUE(type_is_int(type_custom_int_10));
}

TEST(WhereHelpers, TypeIsIntFalseForNonInt) {
  EXPECT_FALSE(type_is_int(type_float));
  EXPECT_FALSE(type_is_int(type_string));
  EXPECT_FALSE(type_is_int(type_invalid));
  EXPECT_FALSE(type_is_int(type_tbd));
  EXPECT_FALSE(type_is_int(type_custom_string));
  EXPECT_FALSE(type_is_int(type_custom_float));
  EXPECT_FALSE(type_is_int(type_custom));
}

TEST(WhereHelpers, TypeIsIntCustomIntEnd) {
  EXPECT_FALSE(type_is_int(type_custom_int_end));
}

// ======================================================================
// type_is_float
// ======================================================================

TEST(WhereHelpers, TypeIsFloatForFloatTypes) {
  EXPECT_TRUE(type_is_float(type_float));
  EXPECT_TRUE(type_is_float(type_int));
  EXPECT_TRUE(type_is_float(type_bool));
  EXPECT_TRUE(type_is_float(type_date));
  EXPECT_TRUE(type_is_float(type_size));
}

TEST(WhereHelpers, TypeIsFloatForCustomInt) {
  EXPECT_TRUE(type_is_float(type_custom_int));
  EXPECT_TRUE(type_is_float(type_custom_int_1));
}

TEST(WhereHelpers, TypeIsFloatForCustomFloat) {
  EXPECT_TRUE(type_is_float(type_custom_float));
  EXPECT_TRUE(type_is_float(type_custom_float_1));
}

TEST(WhereHelpers, TypeIsFloatFalseForNonFloat) {
  EXPECT_FALSE(type_is_float(type_string));
  EXPECT_FALSE(type_is_float(type_invalid));
  EXPECT_FALSE(type_is_float(type_tbd));
  EXPECT_FALSE(type_is_float(type_custom_string));
  EXPECT_FALSE(type_is_float(type_custom));
}

TEST(WhereHelpers, TypeIsFloatCustomIntEnd) {
  EXPECT_FALSE(type_is_float(type_custom_int_end));
}

TEST(WhereHelpers, TypeIsFloatCustomFloatEnd) {
  EXPECT_FALSE(type_is_float(type_custom_float_end));
}

// ======================================================================
// type_is_string
// ======================================================================

TEST(WhereHelpers, TypeIsStringForStringTypes) {
  EXPECT_TRUE(type_is_string(type_string));
}

TEST(WhereHelpers, TypeIsStringForCustomString) {
  EXPECT_TRUE(type_is_string(type_custom_string));
  EXPECT_TRUE(type_is_string(type_custom_string_1));
  EXPECT_TRUE(type_is_string(type_custom_string_9));
}

TEST(WhereHelpers, TypeIsStringFalseForNonString) {
  EXPECT_FALSE(type_is_string(type_int));
  EXPECT_FALSE(type_is_string(type_float));
  EXPECT_FALSE(type_is_string(type_bool));
  EXPECT_FALSE(type_is_string(type_date));
  EXPECT_FALSE(type_is_string(type_size));
  EXPECT_FALSE(type_is_string(type_invalid));
  EXPECT_FALSE(type_is_string(type_tbd));
  EXPECT_FALSE(type_is_string(type_custom_int));
  EXPECT_FALSE(type_is_string(type_custom_float));
  EXPECT_FALSE(type_is_string(type_custom));
}

TEST(WhereHelpers, TypeIsStringCustomStringEnd) {
  EXPECT_FALSE(type_is_string(type_custom_string_end));
}

// ======================================================================
// get_return_type
// ======================================================================

TEST(WhereHelpers, GetReturnTypeInv) {
  EXPECT_EQ(type_int, get_return_type(op_inv, type_int));
  EXPECT_EQ(type_string, get_return_type(op_inv, type_string));
  EXPECT_EQ(type_float, get_return_type(op_inv, type_float));
}

TEST(WhereHelpers, GetReturnTypeBinor) {
  EXPECT_EQ(type_int, get_return_type(op_binor, type_int));
  EXPECT_EQ(type_string, get_return_type(op_binor, type_string));
}

TEST(WhereHelpers, GetReturnTypeBinand) {
  EXPECT_EQ(type_int, get_return_type(op_binand, type_int));
  EXPECT_EQ(type_float, get_return_type(op_binand, type_float));
}

TEST(WhereHelpers, GetReturnTypeComparisonReturnsBool) {
  EXPECT_EQ(type_bool, get_return_type(op_eq, type_int));
  EXPECT_EQ(type_bool, get_return_type(op_lt, type_int));
  EXPECT_EQ(type_bool, get_return_type(op_gt, type_float));
  EXPECT_EQ(type_bool, get_return_type(op_le, type_string));
  EXPECT_EQ(type_bool, get_return_type(op_ge, type_date));
  EXPECT_EQ(type_bool, get_return_type(op_ne, type_size));
  EXPECT_EQ(type_bool, get_return_type(op_and, type_int));
  EXPECT_EQ(type_bool, get_return_type(op_or, type_int));
  EXPECT_EQ(type_bool, get_return_type(op_like, type_string));
  EXPECT_EQ(type_bool, get_return_type(op_in, type_int));
}

// ======================================================================
// operator_to_string
// ======================================================================

TEST(WhereHelpers, OperatorToStringAnd) { EXPECT_EQ("and", operator_to_string(op_and)); }
TEST(WhereHelpers, OperatorToStringOr) { EXPECT_EQ("or", operator_to_string(op_or)); }
TEST(WhereHelpers, OperatorToStringEq) { EXPECT_EQ("=", operator_to_string(op_eq)); }
TEST(WhereHelpers, OperatorToStringGt) { EXPECT_EQ(">", operator_to_string(op_gt)); }
TEST(WhereHelpers, OperatorToStringLt) { EXPECT_EQ("<", operator_to_string(op_lt)); }
TEST(WhereHelpers, OperatorToStringGe) { EXPECT_EQ(">=", operator_to_string(op_ge)); }
TEST(WhereHelpers, OperatorToStringLe) { EXPECT_EQ("<=", operator_to_string(op_le)); }
TEST(WhereHelpers, OperatorToStringIn) { EXPECT_EQ("in", operator_to_string(op_in)); }
TEST(WhereHelpers, OperatorToStringNin) { EXPECT_EQ("not in", operator_to_string(op_nin)); }
TEST(WhereHelpers, OperatorToStringBinand) { EXPECT_EQ("&", operator_to_string(op_binand)); }
TEST(WhereHelpers, OperatorToStringBinor) { EXPECT_EQ("|", operator_to_string(op_binor)); }
TEST(WhereHelpers, OperatorToStringLike) { EXPECT_EQ("like", operator_to_string(op_like)); }

TEST(WhereHelpers, OperatorToStringUnknown) {
  // Operators not explicitly listed should return "?"
  EXPECT_EQ("?", operator_to_string(op_ne));
  EXPECT_EQ("?", operator_to_string(op_not));
  EXPECT_EQ("?", operator_to_string(op_inv));
  EXPECT_EQ("?", operator_to_string(op_regexp));
  EXPECT_EQ("?", operator_to_string(op_not_regexp));
  EXPECT_EQ("?", operator_to_string(op_not_like));
}

// ======================================================================
// can_convert
// ======================================================================

TEST(WhereHelpers, CanConvertInvalidAlwaysFalse) {
  EXPECT_FALSE(can_convert(type_invalid, type_int));
  EXPECT_FALSE(can_convert(type_int, type_invalid));
  EXPECT_FALSE(can_convert(type_invalid, type_invalid));
}

TEST(WhereHelpers, CanConvertToTbdFalse) {
  EXPECT_FALSE(can_convert(type_int, type_tbd));
  EXPECT_FALSE(can_convert(type_string, type_tbd));
}

TEST(WhereHelpers, CanConvertFromTbdTrue) {
  EXPECT_TRUE(can_convert(type_tbd, type_int));
  EXPECT_TRUE(can_convert(type_tbd, type_string));
  EXPECT_TRUE(can_convert(type_tbd, type_float));
}

TEST(WhereHelpers, CanConvertIntToFloat) { EXPECT_TRUE(can_convert(type_int, type_float)); }
TEST(WhereHelpers, CanConvertIntToString) { EXPECT_TRUE(can_convert(type_int, type_string)); }
TEST(WhereHelpers, CanConvertIntToBool) { EXPECT_TRUE(can_convert(type_int, type_bool)); }
TEST(WhereHelpers, CanConvertFloatToInt) { EXPECT_TRUE(can_convert(type_float, type_int)); }
TEST(WhereHelpers, CanConvertFloatToString) { EXPECT_TRUE(can_convert(type_float, type_string)); }
TEST(WhereHelpers, CanConvertFloatToBool) { EXPECT_TRUE(can_convert(type_float, type_bool)); }
TEST(WhereHelpers, CanConvertStringToInt) { EXPECT_TRUE(can_convert(type_string, type_int)); }
TEST(WhereHelpers, CanConvertStringToFloat) { EXPECT_TRUE(can_convert(type_string, type_float)); }
TEST(WhereHelpers, CanConvertBoolToInt) { EXPECT_TRUE(can_convert(type_bool, type_int)); }
TEST(WhereHelpers, CanConvertBoolToFloat) { EXPECT_TRUE(can_convert(type_bool, type_float)); }

TEST(WhereHelpers, CanConvertCustomFloatToFloat) {
  EXPECT_TRUE(can_convert(type_custom_float, type_float));
  EXPECT_TRUE(can_convert(type_custom_float_1, type_float));
}

TEST(WhereHelpers, CanConvertCustomIntToInt) {
  EXPECT_TRUE(can_convert(type_custom_int, type_int));
  EXPECT_TRUE(can_convert(type_custom_int_1, type_int));
}

TEST(WhereHelpers, CanConvertCustomStringToFloat) {
  EXPECT_TRUE(can_convert(type_custom_string, type_float));
  EXPECT_TRUE(can_convert(type_custom_string_1, type_float));
}

TEST(WhereHelpers, CanConvertCustomStringToInt) {
  EXPECT_TRUE(can_convert(type_custom_string, type_int));
  EXPECT_TRUE(can_convert(type_custom_string_1, type_int));
}

TEST(WhereHelpers, CanConvertSameTypeFalse) {
  // Same-type conversions are not listed, so they return false
  EXPECT_FALSE(can_convert(type_int, type_int));
  EXPECT_FALSE(can_convert(type_string, type_string));
  EXPECT_FALSE(can_convert(type_float, type_float));
}

TEST(WhereHelpers, CanConvertStringToBoolFalse) { EXPECT_FALSE(can_convert(type_string, type_bool)); }

TEST(WhereHelpers, CanConvertDateToStringFalse) { EXPECT_FALSE(can_convert(type_date, type_string)); }

TEST(WhereHelpers, CanConvertCustomEndFalse) {
  EXPECT_FALSE(can_convert(type_custom_int_end, type_int));
  EXPECT_FALSE(can_convert(type_custom_string_end, type_float));
  EXPECT_FALSE(can_convert(type_custom_float_end, type_float));
}

// ======================================================================
// is_lower / is_upper
// ======================================================================

TEST(WhereHelpers, IsLowerLe) { EXPECT_TRUE(is_lower(op_le)); }
TEST(WhereHelpers, IsLowerLt) { EXPECT_TRUE(is_lower(op_lt)); }

TEST(WhereHelpers, IsLowerFalseForOthers) {
  EXPECT_FALSE(is_lower(op_ge));
  EXPECT_FALSE(is_lower(op_gt));
  EXPECT_FALSE(is_lower(op_eq));
  EXPECT_FALSE(is_lower(op_ne));
  EXPECT_FALSE(is_lower(op_and));
  EXPECT_FALSE(is_lower(op_or));
}

TEST(WhereHelpers, IsUpperGe) { EXPECT_TRUE(is_upper(op_ge)); }
TEST(WhereHelpers, IsUpperGt) { EXPECT_TRUE(is_upper(op_gt)); }

TEST(WhereHelpers, IsUpperFalseForOthers) {
  EXPECT_FALSE(is_upper(op_le));
  EXPECT_FALSE(is_upper(op_lt));
  EXPECT_FALSE(is_upper(op_eq));
  EXPECT_FALSE(is_upper(op_ne));
  EXPECT_FALSE(is_upper(op_and));
  EXPECT_FALSE(is_upper(op_or));
}

// ======================================================================
// type_to_string — additional edge cases
// ======================================================================

TEST(WhereHelpers, TypeToStringMulti) {
  // type_multi (88) is not explicitly listed, falls through to custom range checks
  // Since type_multi=88 is below type_custom_int=1024, it should return "unknown:88"
  EXPECT_EQ("unknown:88", type_to_string(type_multi));
}

// ======================================================================
// operator_to_string — ne operator
// ======================================================================

TEST(WhereHelpers, OperatorToStringNe) {
  // op_ne is not in the mapping, should return "?"
  EXPECT_EQ("?", operator_to_string(op_ne));
}

// ======================================================================
// get_return_type — additional operators
// ======================================================================

TEST(WhereHelpers, GetReturnTypeNin) { EXPECT_EQ(type_bool, get_return_type(op_nin, type_int)); }

TEST(WhereHelpers, GetReturnTypeNot) { EXPECT_EQ(type_bool, get_return_type(op_not, type_bool)); }

TEST(WhereHelpers, GetReturnTypeRegexp) { EXPECT_EQ(type_bool, get_return_type(op_regexp, type_string)); }

TEST(WhereHelpers, GetReturnTypeNotRegexp) { EXPECT_EQ(type_bool, get_return_type(op_not_regexp, type_string)); }

TEST(WhereHelpers, GetReturnTypeNotLike) { EXPECT_EQ(type_bool, get_return_type(op_not_like, type_string)); }

TEST(WhereHelpers, GetReturnTypeInvPreservesAllTypes) {
  EXPECT_EQ(type_bool, get_return_type(op_inv, type_bool));
  EXPECT_EQ(type_date, get_return_type(op_inv, type_date));
  EXPECT_EQ(type_size, get_return_type(op_inv, type_size));
  EXPECT_EQ(type_invalid, get_return_type(op_inv, type_invalid));
  EXPECT_EQ(type_tbd, get_return_type(op_inv, type_tbd));
}

TEST(WhereHelpers, GetReturnTypeBinorPreservesAllTypes) {
  EXPECT_EQ(type_bool, get_return_type(op_binor, type_bool));
  EXPECT_EQ(type_float, get_return_type(op_binor, type_float));
  EXPECT_EQ(type_date, get_return_type(op_binor, type_date));
}

TEST(WhereHelpers, GetReturnTypeBinandPreservesAllTypes) {
  EXPECT_EQ(type_bool, get_return_type(op_binand, type_bool));
  EXPECT_EQ(type_string, get_return_type(op_binand, type_string));
  EXPECT_EQ(type_size, get_return_type(op_binand, type_size));
}

// ======================================================================
// can_convert — additional negative cases
// ======================================================================

TEST(WhereHelpers, CanConvertBoolToStringFalse) { EXPECT_FALSE(can_convert(type_bool, type_string)); }

TEST(WhereHelpers, CanConvertDateToIntFalse) { EXPECT_FALSE(can_convert(type_date, type_int)); }

TEST(WhereHelpers, CanConvertDateToFloatFalse) { EXPECT_FALSE(can_convert(type_date, type_float)); }

TEST(WhereHelpers, CanConvertDateToBoolFalse) { EXPECT_FALSE(can_convert(type_date, type_bool)); }

TEST(WhereHelpers, CanConvertSizeToStringFalse) { EXPECT_FALSE(can_convert(type_size, type_string)); }

TEST(WhereHelpers, CanConvertSizeToIntFalse) { EXPECT_FALSE(can_convert(type_size, type_int)); }

TEST(WhereHelpers, CanConvertCustomFloatToIntFalse) { EXPECT_FALSE(can_convert(type_custom_float, type_int)); }

TEST(WhereHelpers, CanConvertCustomFloatToStringFalse) { EXPECT_FALSE(can_convert(type_custom_float, type_string)); }

TEST(WhereHelpers, CanConvertCustomIntToFloatFalse) { EXPECT_FALSE(can_convert(type_custom_int, type_float)); }

TEST(WhereHelpers, CanConvertCustomIntToStringFalse) { EXPECT_FALSE(can_convert(type_custom_int, type_string)); }

TEST(WhereHelpers, CanConvertCustomStringToStringFalse) { EXPECT_FALSE(can_convert(type_custom_string, type_string)); }

TEST(WhereHelpers, CanConvertCustomStringToBoolFalse) { EXPECT_FALSE(can_convert(type_custom_string, type_bool)); }

TEST(WhereHelpers, CanConvertCustomToAnythingFalse) {
  // type_custom is not in any conversion rule as source
  EXPECT_FALSE(can_convert(type_custom, type_int));
  EXPECT_FALSE(can_convert(type_custom, type_float));
  EXPECT_FALSE(can_convert(type_custom, type_string));
}

TEST(WhereHelpers, CanConvertFromTbdToInvalidFalse) {
  // src=tbd should return true for most, but dst=invalid returns false first
  EXPECT_FALSE(can_convert(type_tbd, type_invalid));
}

TEST(WhereHelpers, CanConvertTbdToTbdFalse) {
  // dst=tbd check comes before src=tbd check
  EXPECT_FALSE(can_convert(type_tbd, type_tbd));
}

// ======================================================================
// is_lower / is_upper — additional operators
// ======================================================================

TEST(WhereHelpers, IsLowerFalseForBitwise) {
  EXPECT_FALSE(is_lower(op_binand));
  EXPECT_FALSE(is_lower(op_binor));
}

TEST(WhereHelpers, IsUpperFalseForBitwise) {
  EXPECT_FALSE(is_upper(op_binand));
  EXPECT_FALSE(is_upper(op_binor));
}

TEST(WhereHelpers, IsLowerFalseForLike) {
  EXPECT_FALSE(is_lower(op_like));
  EXPECT_FALSE(is_lower(op_not_like));
  EXPECT_FALSE(is_lower(op_in));
  EXPECT_FALSE(is_lower(op_nin));
}

TEST(WhereHelpers, IsUpperFalseForLike) {
  EXPECT_FALSE(is_upper(op_like));
  EXPECT_FALSE(is_upper(op_not_like));
  EXPECT_FALSE(is_upper(op_in));
  EXPECT_FALSE(is_upper(op_nin));
}

// ======================================================================
// add_convert_node
// ======================================================================

TEST(WhereHelpers, AddConvertNodeSameTypeReturnsSameNode) {
  node_type n = factory::create_int(42);
  node_type result = add_convert_node(n, type_int);
  EXPECT_EQ(n.get(), result.get());
  EXPECT_EQ(type_int, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeSameTypeStringReturnsSameNode) {
  node_type n = factory::create_string("hello");
  node_type result = add_convert_node(n, type_string);
  EXPECT_EQ(n.get(), result.get());
  EXPECT_EQ(type_string, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeSameTypeFloatReturnsSameNode) {
  node_type n = factory::create_float(3.14);
  node_type result = add_convert_node(n, type_float);
  EXPECT_EQ(n.get(), result.get());
  EXPECT_EQ(type_float, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeDifferentTypeCreatesConversion) {
  node_type n = factory::create_int(42);
  node_type result = add_convert_node(n, type_float);
  // Should return a new (conversion) node, not the original
  EXPECT_NE(n.get(), result.get());
  EXPECT_EQ(type_float, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeIntToStringCreatesConversion) {
  node_type n = factory::create_int(100);
  node_type result = add_convert_node(n, type_string);
  EXPECT_NE(n.get(), result.get());
  EXPECT_EQ(type_string, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeFloatToIntCreatesConversion) {
  node_type n = factory::create_float(2.5);
  node_type result = add_convert_node(n, type_int);
  EXPECT_NE(n.get(), result.get());
  EXPECT_EQ(type_int, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeStringToIntCreatesConversion) {
  node_type n = factory::create_string("123");
  node_type result = add_convert_node(n, type_int);
  EXPECT_NE(n.get(), result.get());
  EXPECT_EQ(type_int, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeTbdDigestsNewType) {
  // A node with type_tbd should absorb the new type without creating a conversion node
  node_type n = factory::create_int(42);
  n->set_type(type_tbd);
  EXPECT_EQ(type_tbd, n->get_type());

  node_type result = add_convert_node(n, type_float);
  // Should return the same node (not a new conversion node) with updated type
  EXPECT_EQ(n.get(), result.get());
  EXPECT_EQ(type_float, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeTbdToStringDigests) {
  node_type n = factory::create_string("test");
  n->set_type(type_tbd);

  node_type result = add_convert_node(n, type_string);
  // type == newtype check passes first since we set type to tbd and newtype is string
  // Actually: type_tbd != type_string, and type == type_tbd, so it digests
  EXPECT_EQ(n.get(), result.get());
  EXPECT_EQ(type_string, result->get_type());
}

TEST(WhereHelpers, AddConvertNodeTbdToIntDigests) {
  node_type n = factory::create_float(1.0);
  n->set_type(type_tbd);

  node_type result = add_convert_node(n, type_int);
  EXPECT_EQ(n.get(), result.get());
  EXPECT_EQ(type_int, result->get_type());
}

// ======================================================================
// infer_binary_type
// ======================================================================

TEST(WhereHelpers, InferBinaryTypeSameTypeInt) {
  auto converter = make_converter();
  node_type left = factory::create_int(1);
  node_type right = factory::create_int(2);
  EXPECT_EQ(type_int, infer_binary_type(converter, left, right));
}

TEST(WhereHelpers, InferBinaryTypeSameTypeString) {
  auto converter = make_converter();
  node_type left = factory::create_string("a");
  node_type right = factory::create_string("b");
  EXPECT_EQ(type_string, infer_binary_type(converter, left, right));
}

TEST(WhereHelpers, InferBinaryTypeSameTypeFloat) {
  auto converter = make_converter();
  node_type left = factory::create_float(1.0);
  node_type right = factory::create_float(2.0);
  EXPECT_EQ(type_float, infer_binary_type(converter, left, right));
}

TEST(WhereHelpers, InferBinaryTypeFloatAndIntConverts) {
  auto converter = make_converter();
  node_type left = factory::create_float(1.0);
  node_type right = factory::create_int(2);
  value_type result = infer_binary_type(converter, left, right);
  // float + int: type_is_float(lt) && type_is_int(rt) triggers right->infer_type(factory, lt)
  // int_value::infer_type with float suggestion sets type to float
  EXPECT_EQ(type_float, result);
}

TEST(WhereHelpers, InferBinaryTypeIntAndFloatConverts) {
  auto converter = make_converter();
  node_type left = factory::create_int(1);
  node_type right = factory::create_float(2.0);
  value_type result = infer_binary_type(converter, left, right);
  // int + float: type_is_float(rt) && type_is_int(lt) triggers left->infer_type(factory, rt)
  EXPECT_EQ(type_float, result);
}

TEST(WhereHelpers, InferBinaryTypeWithCanConvertOnConverter) {
  // When types differ and built-in can_convert fails, the converter's can_convert is tried
  auto converter = make_converter(true);
  node_type left = factory::create_string("hello");
  node_type right = factory::create_int(42);
  value_type result = infer_binary_type(converter, left, right);
  // converter->can_convert(rt=int, lt=string) returns true, so right gets converted to string
  EXPECT_EQ(type_string, result);
}



TEST(WhereHelpers, InferBinaryTypeStringAndIntUsesBuiltinConvert) {
  auto converter = make_converter(false);
  node_type left = factory::create_string("42");
  node_type right = factory::create_int(42);
  // can_convert(type_int, type_string) is true, so right is converted to string
  value_type result = infer_binary_type(converter, left, right);
  EXPECT_EQ(type_string, result);
}

// ======================================================================
// read_arguments
// ======================================================================

TEST(WhereHelpers, ReadArgumentsSingleIntNode) {
  auto ctx = make_eval_context();
  node_type n = factory::create_int(42);
  read_arg_type result = read_arguments(ctx, n, "bytes");
  EXPECT_EQ(42, boost::get<0>(result));
  EXPECT_DOUBLE_EQ(42.0, boost::get<1>(result));
  EXPECT_EQ("bytes", boost::get<2>(result));
}

TEST(WhereHelpers, ReadArgumentsSingleFloatNode) {
  auto ctx = make_eval_context();
  node_type n = factory::create_float(3.14);
  read_arg_type result = read_arguments(ctx, n, "sec");
  EXPECT_EQ(3, boost::get<0>(result));
  EXPECT_DOUBLE_EQ(3.14, boost::get<1>(result));
  EXPECT_EQ("sec", boost::get<2>(result));
}

TEST(WhereHelpers, ReadArgumentsDefaultUnit) {
  auto ctx = make_eval_context();
  node_type n = factory::create_int(100);
  read_arg_type result = read_arguments(ctx, n, "MB");
  EXPECT_EQ("MB", boost::get<2>(result));
}

TEST(WhereHelpers, ReadArgumentsListWithUnit) {
  auto ctx = make_eval_context();
  list_node_type list = factory::create_list();
  list->push_back(factory::create_int(1024));
  list->push_back(factory::create_string("KB"));
  read_arg_type result = read_arguments(ctx, list, "bytes");
  EXPECT_EQ(1024, boost::get<0>(result));
  EXPECT_DOUBLE_EQ(1024.0, boost::get<1>(result));
  EXPECT_EQ("KB", boost::get<2>(result));
}

TEST(WhereHelpers, ReadArgumentsListWithFloatAndUnit) {
  auto ctx = make_eval_context();
  list_node_type list = factory::create_list();
  list->push_back(factory::create_float(2.5));
  list->push_back(factory::create_string("GB"));
  read_arg_type result = read_arguments(ctx, list, "MB");
  EXPECT_EQ(2, boost::get<0>(result));
  EXPECT_DOUBLE_EQ(2.5, boost::get<1>(result));
  EXPECT_EQ("GB", boost::get<2>(result));
}

TEST(WhereHelpers, ReadArgumentsZeroValue) {
  auto ctx = make_eval_context();
  node_type n = factory::create_int(0);
  read_arg_type result = read_arguments(ctx, n, "ms");
  EXPECT_EQ(0, boost::get<0>(result));
  EXPECT_DOUBLE_EQ(0.0, boost::get<1>(result));
  EXPECT_EQ("ms", boost::get<2>(result));
}

TEST(WhereHelpers, ReadArgumentsNegativeValue) {
  auto ctx = make_eval_context();
  node_type n = factory::create_int(-5);
  read_arg_type result = read_arguments(ctx, n, "sec");
  EXPECT_EQ(-5, boost::get<0>(result));
  EXPECT_DOUBLE_EQ(-5.0, boost::get<1>(result));
  EXPECT_EQ("sec", boost::get<2>(result));
}

TEST(WhereHelpers, ReadArgumentsEmptyDefaultUnit) {
  auto ctx = make_eval_context();
  node_type n = factory::create_int(10);
  read_arg_type result = read_arguments(ctx, n, "");
  EXPECT_EQ("", boost::get<2>(result));
}

