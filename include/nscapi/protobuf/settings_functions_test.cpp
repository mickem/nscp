/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <nscapi/protobuf/settings_functions.hpp>
#include <string>

using namespace nscapi::protobuf::functions;

// ============================================================================
// key_values Tests
// ============================================================================

class KeyValuesTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Constructor tests
TEST_F(KeyValuesTest, ConstructWithPathOnly) {
  const settings_query::key_values kv("/test/path");
  EXPECT_EQ("/test/path", kv.path());
  EXPECT_EQ("", kv.key());
  EXPECT_EQ("", kv.get_string());
}

TEST_F(KeyValuesTest, ConstructWithStringValue) {
  const settings_query::key_values kv("/test/path", "mykey", std::string("myvalue"));
  EXPECT_EQ("/test/path", kv.path());
  EXPECT_EQ("mykey", kv.key());
  EXPECT_EQ("myvalue", kv.get_string());
}

TEST_F(KeyValuesTest, ConstructWithIntValue) {
  const settings_query::key_values kv("/test/path", "mykey", 42LL);
  EXPECT_EQ("/test/path", kv.path());
  EXPECT_EQ("mykey", kv.key());
  EXPECT_EQ(42, kv.get_int());
  EXPECT_EQ("42", kv.get_string());
}

TEST_F(KeyValuesTest, ConstructWithBoolValueTrue) {
  const settings_query::key_values kv("/test/path", "mykey", true);
  EXPECT_EQ("/test/path", kv.path());
  EXPECT_EQ("mykey", kv.key());
  EXPECT_TRUE(kv.get_bool());
  EXPECT_EQ("true", kv.get_string());
}

TEST_F(KeyValuesTest, ConstructWithBoolValueFalse) {
  const settings_query::key_values kv("/test/path", "mykey", false);
  EXPECT_EQ("/test/path", kv.path());
  EXPECT_EQ("mykey", kv.key());
  EXPECT_FALSE(kv.get_bool());
  EXPECT_EQ("false", kv.get_string());
}

// Copy constructor and assignment operator tests
TEST_F(KeyValuesTest, CopyConstructor) {
  const settings_query::key_values kv1("/test/path", "mykey", std::string("myvalue"));
  const settings_query::key_values kv2(kv1);
  EXPECT_EQ(kv1.path(), kv2.path());
  EXPECT_EQ(kv1.key(), kv2.key());
  EXPECT_EQ(kv1.get_string(), kv2.get_string());
}

TEST_F(KeyValuesTest, AssignmentOperator) {
  const settings_query::key_values kv1("/test/path1", "key1", std::string("value1"));
  settings_query::key_values kv2("/test/path2", "key2", std::string("value2"));
  kv2 = kv1;
  EXPECT_EQ(kv1.path(), kv2.path());
  EXPECT_EQ(kv1.key(), kv2.key());
  EXPECT_EQ(kv1.get_string(), kv2.get_string());
}

// matches() tests
TEST_F(KeyValuesTest, MatchesPathAndKeyString) {
  const settings_query::key_values kv("/test/path", "mykey", std::string("myvalue"));
  EXPECT_TRUE(kv.matches(std::string("/test/path"), std::string("mykey")));
  EXPECT_FALSE(kv.matches(std::string("/test/path"), std::string("wrongkey")));
  EXPECT_FALSE(kv.matches(std::string("/wrong/path"), std::string("mykey")));
}

TEST_F(KeyValuesTest, MatchesPathAndKeyCharPtr) {
  const settings_query::key_values kv("/test/path", "mykey", std::string("myvalue"));
  EXPECT_TRUE(kv.matches("/test/path", "mykey"));
  EXPECT_FALSE(kv.matches("/test/path", "wrongkey"));
  EXPECT_FALSE(kv.matches("/wrong/path", "mykey"));
}

TEST_F(KeyValuesTest, MatchesPathOnlyString) {
  const settings_query::key_values kv("/test/path", "mykey", std::string("myvalue"));
  EXPECT_TRUE(kv.matches(std::string("/test/path")));
  EXPECT_FALSE(kv.matches(std::string("/wrong/path")));
}

TEST_F(KeyValuesTest, MatchesPathOnlyCharPtr) {
  const settings_query::key_values kv("/test/path", "mykey", std::string("myvalue"));
  EXPECT_TRUE(kv.matches("/test/path"));
  EXPECT_FALSE(kv.matches("/wrong/path"));
}

TEST_F(KeyValuesTest, MatchesWithPathOnlyConstructor) {
  const settings_query::key_values kv("/test/path");
  // When constructed with path only, key is not set, so matches with key should return false
  EXPECT_FALSE(kv.matches("/test/path", "anykey"));
  EXPECT_FALSE(kv.matches(std::string("/test/path"), std::string("anykey")));
  // Note: There's an inconsistency in the implementation:
  // - matches(const char* path) returns false when key is not set
  // - matches(const std::string& path) returns true when path matches
  EXPECT_FALSE(kv.matches("/test/path"));              // const char* version requires key
  EXPECT_TRUE(kv.matches(std::string("/test/path")));  // std::string version only checks path
}

// Type conversion tests
TEST_F(KeyValuesTest, GetIntFromStringValue) {
  const settings_query::key_values kv("/test/path", "mykey", std::string("123"));
  EXPECT_EQ(123, kv.get_int());
}

TEST_F(KeyValuesTest, GetIntFromBoolValue) {
  const settings_query::key_values kv_true("/test/path", "mykey", true);
  EXPECT_EQ(1, kv_true.get_int());

  const settings_query::key_values kv_false("/test/path", "mykey", false);
  EXPECT_EQ(0, kv_false.get_int());
}

TEST_F(KeyValuesTest, GetBoolFromStringValueTrue) {
  const settings_query::key_values kv1("/test/path", "mykey", std::string("true"));
  EXPECT_TRUE(kv1.get_bool());

  const settings_query::key_values kv2("/test/path", "mykey", std::string("TRUE"));
  EXPECT_TRUE(kv2.get_bool());

  const settings_query::key_values kv3("/test/path", "mykey", std::string("1"));
  EXPECT_TRUE(kv3.get_bool());
}

TEST_F(KeyValuesTest, GetBoolFromStringValueFalse) {
  const settings_query::key_values kv1("/test/path", "mykey", std::string("false"));
  EXPECT_FALSE(kv1.get_bool());

  const settings_query::key_values kv2("/test/path", "mykey", std::string("0"));
  EXPECT_FALSE(kv2.get_bool());

  const settings_query::key_values kv3("/test/path", "mykey", std::string("anything"));
  EXPECT_FALSE(kv3.get_bool());
}

TEST_F(KeyValuesTest, GetBoolFromIntValue) {
  const settings_query::key_values kv_one("/test/path", "mykey", 1LL);
  EXPECT_TRUE(kv_one.get_bool());

  const settings_query::key_values kv_zero("/test/path", "mykey", 0LL);
  EXPECT_FALSE(kv_zero.get_bool());

  const settings_query::key_values kv_other("/test/path", "mykey", 42LL);
  EXPECT_FALSE(kv_other.get_bool());
}

TEST_F(KeyValuesTest, GetStringFromIntValue) {
  const settings_query::key_values kv("/test/path", "mykey", 12345LL);
  EXPECT_EQ("12345", kv.get_string());
}

TEST_F(KeyValuesTest, GetStringFromBoolValue) {
  const settings_query::key_values kv_true("/test/path", "mykey", true);
  EXPECT_EQ("true", kv_true.get_string());

  const settings_query::key_values kv_false("/test/path", "mykey", false);
  EXPECT_EQ("false", kv_false.get_string());
}

// ============================================================================
// settings_query Tests
// ============================================================================

class SettingsQueryTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Constructor and destructor tests
TEST_F(SettingsQueryTest, ConstructWithPluginId) {
  settings_query query(42);
  // Should be able to create and destroy without issues
  SUCCEED();
}

// Request serialization tests
TEST_F(SettingsQueryTest, SetGeneratesRequest) {
  settings_query query(1);
  query.set("/test/path", "mykey", "myvalue");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, GetStringGeneratesRequest) {
  settings_query query(1);
  query.get("/test/path", "mykey", std::string("default"));
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, GetCharPtrGeneratesRequest) {
  settings_query query(1);
  query.get("/test/path", "mykey", "default");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, GetLongLongGeneratesRequest) {
  settings_query query(1);
  query.get("/test/path", "mykey", 100LL);
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, GetBoolGeneratesRequest) {
  settings_query query(1);
  query.get("/test/path", "mykey", true);
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, EraseGeneratesRequest) {
  settings_query query(1);
  query.erase("/test/path", "mykey");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, ListGeneratesRequest) {
  settings_query query(1);
  query.list("/test/path");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, ListRecursiveGeneratesRequest) {
  settings_query query(1);
  query.list("/test/path", true);
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, SaveGeneratesRequest) {
  settings_query query(1);
  query.save();
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, LoadGeneratesRequest) {
  settings_query query(1);
  query.load();
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryTest, ReloadGeneratesRequest) {
  settings_query query(1);
  query.reload();
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

// Multiple operations test
TEST_F(SettingsQueryTest, MultipleOperationsGenerateRequest) {
  settings_query query(1);
  query.set("/path1", "key1", "value1");
  query.get("/path2", "key2", std::string("default2"));
  query.list("/path3");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

// Response buffer access test
TEST_F(SettingsQueryTest, ResponseBufferAccessible) {
  const settings_query query(1);
  std::string& response = query.response();
  response = "test response data";
  EXPECT_EQ("test response data", query.response());
}

// Empty response validation test
TEST_F(SettingsQueryTest, ValidateEmptyResponse) {
  const settings_query query(1);
  // Empty response should validate as true (no errors)
  EXPECT_TRUE(query.validate_response());
}

TEST_F(SettingsQueryTest, GetResponseErrorEmpty) {
  const settings_query query(1);
  // Empty response should have empty error message
  EXPECT_EQ("", query.get_response_error());
}

TEST_F(SettingsQueryTest, GetQueryKeyResponseEmpty) {
  const settings_query query(1);
  // Empty response should return empty list
  const std::list<settings_query::key_values> results = query.get_query_key_response();
  EXPECT_TRUE(results.empty());
}

// ============================================================================
// Edge case tests
// ============================================================================

class SettingsQueryEdgeCaseTest : public ::testing::Test {};

TEST_F(SettingsQueryEdgeCaseTest, EmptyPathAndKey) {
  const settings_query::key_values kv("", "", std::string(""));
  EXPECT_EQ("", kv.path());
  EXPECT_EQ("", kv.key());
  EXPECT_EQ("", kv.get_string());
}

TEST_F(SettingsQueryEdgeCaseTest, SpecialCharactersInPath) {
  const settings_query::key_values kv("/path/with spaces/and-dashes/and_underscores", "key", std::string("value"));
  EXPECT_EQ("/path/with spaces/and-dashes/and_underscores", kv.path());
}

TEST_F(SettingsQueryEdgeCaseTest, UnicodeInValue) {
  const settings_query::key_values kv("/test/path", "key", std::string("value with unicode: ü ö ä"));
  EXPECT_EQ("value with unicode: ü ö ä", kv.get_string());
}

TEST_F(SettingsQueryEdgeCaseTest, LargeIntValue) {
  constexpr long long largeValue = 9223372036854775807LL;  // Max value for long long
  const settings_query::key_values kv("/test/path", "key", largeValue);
  EXPECT_EQ(largeValue, kv.get_int());
}

TEST_F(SettingsQueryEdgeCaseTest, NegativeIntValue) {
  constexpr long long negativeValue = -9223372036854775807LL;
  const settings_query::key_values kv("/test/path", "key", negativeValue);
  EXPECT_EQ(negativeValue, kv.get_int());
}

TEST_F(SettingsQueryEdgeCaseTest, ZeroIntValue) {
  const settings_query::key_values kv("/test/path", "key", 0LL);
  EXPECT_EQ(0, kv.get_int());
  EXPECT_EQ("0", kv.get_string());
}

TEST_F(SettingsQueryEdgeCaseTest, ZeroPluginId) {
  settings_query query(0);
  query.get("/test/path", "key", std::string("default"));
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryEdgeCaseTest, NegativePluginId) {
  settings_query query(-1);
  query.get("/test/path", "key", std::string("default"));
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

// Test that get_bool returns false (not "") when no value is set
TEST_F(SettingsQueryEdgeCaseTest, GetBoolReturnsProperFalse) {
  const settings_query::key_values kv("/test/path");
  // Should return false, not crash or return unexpected value
  EXPECT_FALSE(kv.get_bool());
}

// Test that get_int returns 0 when no value is set
TEST_F(SettingsQueryEdgeCaseTest, GetIntReturnsZeroWhenNoValue) {
  const settings_query::key_values kv("/test/path");
  EXPECT_EQ(0, kv.get_int());
}

// Test that get_string returns empty string when no value is set
TEST_F(SettingsQueryEdgeCaseTest, GetStringReturnsEmptyWhenNoValue) {
  const settings_query::key_values kv("/test/path");
  EXPECT_EQ("", kv.get_string());
}

// Test self-assignment doesn't cause issues
TEST_F(SettingsQueryEdgeCaseTest, SelfAssignment) {
  settings_query::key_values kv("/test/path", "key", std::string("value"));
  kv = kv;  // Self-assignment
  EXPECT_EQ("/test/path", kv.path());
  EXPECT_EQ("key", kv.key());
  EXPECT_EQ("value", kv.get_string());
}

// ============================================================================
// Additional Type Conversion Tests
// ============================================================================

class TypeConversionTest : public ::testing::Test {};

TEST_F(TypeConversionTest, GetBoolFromMixedCaseTrue) {
  const settings_query::key_values kv1("/test/path", "key", std::string("True"));
  EXPECT_TRUE(kv1.get_bool());

  const settings_query::key_values kv2("/test/path", "key", std::string("tRuE"));
  EXPECT_TRUE(kv2.get_bool());

  const settings_query::key_values kv3("/test/path", "key", std::string("TrUe"));
  EXPECT_TRUE(kv3.get_bool());
}

TEST_F(TypeConversionTest, GetBoolFromWhitespaceString) {
  const settings_query::key_values kv("/test/path", "key", std::string("  true  "));
  // Whitespace not trimmed, should return false
  EXPECT_FALSE(kv.get_bool());
}

TEST_F(TypeConversionTest, GetBoolFromEmptyString) {
  const settings_query::key_values kv("/test/path", "key", std::string(""));
  EXPECT_FALSE(kv.get_bool());
}

TEST_F(TypeConversionTest, GetIntFromNegativeString) {
  const settings_query::key_values kv("/test/path", "key", std::string("-456"));
  EXPECT_EQ(-456, kv.get_int());
}

TEST_F(TypeConversionTest, GetIntFromZeroString) {
  const settings_query::key_values kv("/test/path", "key", std::string("0"));
  EXPECT_EQ(0, kv.get_int());
}

TEST_F(TypeConversionTest, GetStringFromNegativeInt) {
  const settings_query::key_values kv("/test/path", "key", -999LL);
  EXPECT_EQ("-999", kv.get_string());
}

TEST_F(TypeConversionTest, GetStringFromZeroInt) {
  const settings_query::key_values kv("/test/path", "key", 0LL);
  EXPECT_EQ("0", kv.get_string());
}

// ============================================================================
// Copy Constructor Tests for Different Value Types
// ============================================================================

class CopyConstructorTest : public ::testing::Test {};

TEST_F(CopyConstructorTest, CopyIntValue) {
  const settings_query::key_values kv1("/test/path", "key", 12345LL);
  const settings_query::key_values kv2(kv1);
  EXPECT_EQ(kv1.path(), kv2.path());
  EXPECT_EQ(kv1.key(), kv2.key());
  EXPECT_EQ(kv1.get_int(), kv2.get_int());
}

TEST_F(CopyConstructorTest, CopyBoolValue) {
  const settings_query::key_values kv1("/test/path", "key", true);
  const settings_query::key_values kv2(kv1);
  EXPECT_EQ(kv1.path(), kv2.path());
  EXPECT_EQ(kv1.key(), kv2.key());
  EXPECT_EQ(kv1.get_bool(), kv2.get_bool());
}

TEST_F(CopyConstructorTest, CopyPathOnly) {
  const settings_query::key_values kv1("/test/path");
  const settings_query::key_values kv2(kv1);
  EXPECT_EQ(kv1.path(), kv2.path());
  EXPECT_EQ(kv1.key(), kv2.key());
}

// ============================================================================
// Assignment Operator Tests for Different Value Types
// ============================================================================

class AssignmentOperatorTest : public ::testing::Test {};

TEST_F(AssignmentOperatorTest, AssignIntValue) {
  const settings_query::key_values kv1("/path1", "key1", 100LL);
  settings_query::key_values kv2("/path2", "key2", std::string("value2"));
  kv2 = kv1;
  EXPECT_EQ(kv1.path(), kv2.path());
  EXPECT_EQ(kv1.key(), kv2.key());
  EXPECT_EQ(kv1.get_int(), kv2.get_int());
}

TEST_F(AssignmentOperatorTest, AssignBoolValue) {
  const settings_query::key_values kv1("/path1", "key1", true);
  settings_query::key_values kv2("/path2", "key2", false);
  kv2 = kv1;
  EXPECT_EQ(kv1.path(), kv2.path());
  EXPECT_EQ(kv1.key(), kv2.key());
  EXPECT_EQ(kv1.get_bool(), kv2.get_bool());
}

TEST_F(AssignmentOperatorTest, AssignFromPathOnlyToFull) {
  const settings_query::key_values kv1("/path1");
  settings_query::key_values kv2("/path2", "key2", std::string("value2"));
  kv2 = kv1;
  EXPECT_EQ("/path1", kv2.path());
  EXPECT_EQ("", kv2.key());
}

TEST_F(AssignmentOperatorTest, AssignFromFullToPathOnly) {
  const settings_query::key_values kv1("/path1", "key1", std::string("value1"));
  settings_query::key_values kv2("/path2");
  kv2 = kv1;
  EXPECT_EQ("/path1", kv2.path());
  EXPECT_EQ("key1", kv2.key());
  EXPECT_EQ("value1", kv2.get_string());
}

// ============================================================================
// Matches Method Edge Cases
// ============================================================================

class MatchesEdgeCaseTest : public ::testing::Test {};

TEST_F(MatchesEdgeCaseTest, MatchesEmptyPathAndKey) {
  const settings_query::key_values kv("", "", std::string("value"));
  EXPECT_TRUE(kv.matches("", ""));
  EXPECT_TRUE(kv.matches(std::string(""), std::string("")));
}

TEST_F(MatchesEdgeCaseTest, MatchesEmptyPath) {
  const settings_query::key_values kv("", "key", std::string("value"));
  EXPECT_TRUE(kv.matches(""));
  EXPECT_TRUE(kv.matches(std::string("")));
}

TEST_F(MatchesEdgeCaseTest, MatchesWithSpecialCharacters) {
  const settings_query::key_values kv("/path/with!@#$%^&*()", "key!@#", std::string("value"));
  EXPECT_TRUE(kv.matches("/path/with!@#$%^&*()", "key!@#"));
  EXPECT_FALSE(kv.matches("/path/with!@#$%^&*()", "key"));
}

TEST_F(MatchesEdgeCaseTest, MatchesWithSlashes) {
  const settings_query::key_values kv("/a/b/c/d/e", "key/with/slashes", std::string("value"));
  EXPECT_TRUE(kv.matches("/a/b/c/d/e", "key/with/slashes"));
}

TEST_F(MatchesEdgeCaseTest, MatchesCaseSensitive) {
  const settings_query::key_values kv("/Test/Path", "MyKey", std::string("value"));
  EXPECT_TRUE(kv.matches("/Test/Path", "MyKey"));
  EXPECT_FALSE(kv.matches("/test/path", "mykey"));
  EXPECT_FALSE(kv.matches("/TEST/PATH", "MYKEY"));
}

// ============================================================================
// Settings Query Operation Chaining Tests
// ============================================================================

class SettingsQueryChainingTest : public ::testing::Test {};

TEST_F(SettingsQueryChainingTest, MultipleSetOperations) {
  settings_query query(1);
  query.set("/path1", "key1", "value1");
  query.set("/path2", "key2", "value2");
  query.set("/path3", "key3", "value3");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryChainingTest, MultipleGetOperations) {
  settings_query query(1);
  query.get("/path1", "key1", std::string("default1"));
  query.get("/path2", "key2", 100LL);
  query.get("/path3", "key3", true);
  query.get("/path4", "key4", "default4");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryChainingTest, MixedOperations) {
  settings_query query(1);
  query.set("/path1", "key1", "value1");
  query.get("/path2", "key2", std::string("default2"));
  query.erase("/path3", "key3");
  query.list("/path4");
  query.list("/path5", true);
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryChainingTest, ControlOperations) {
  settings_query query(1);
  query.save();
  query.load();
  query.reload();
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

// ============================================================================
// Settings Query with Various Path Formats
// ============================================================================

class SettingsQueryPathFormatTest : public ::testing::Test {};

TEST_F(SettingsQueryPathFormatTest, EmptyPath) {
  settings_query query(1);
  query.get("", "key", std::string("default"));
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryPathFormatTest, RootPath) {
  settings_query query(1);
  query.get("/", "key", std::string("default"));
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryPathFormatTest, DeepNestedPath) {
  settings_query query(1);
  query.get("/a/b/c/d/e/f/g/h/i/j", "key", std::string("default"));
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryPathFormatTest, PathWithNumbers) {
  settings_query query(1);
  query.get("/path/123/456", "key789", std::string("default"));
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryPathFormatTest, PathWithSpaces) {
  settings_query query(1);
  query.get("/path with spaces/more spaces", "key with space", std::string("default value"));
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

// ============================================================================
// Settings Query List Operation Tests
// ============================================================================

class SettingsQueryListTest : public ::testing::Test {};

TEST_F(SettingsQueryListTest, ListNonRecursive) {
  settings_query query(1);
  query.list("/test/path", false);
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryListTest, ListRecursive) {
  settings_query query(1);
  query.list("/test/path", true);
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryListTest, ListDefaultNonRecursive) {
  settings_query query(1);
  query.list("/test/path");  // Default should be false
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryListTest, ListEmptyPath) {
  settings_query query(1);
  query.list("");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

TEST_F(SettingsQueryListTest, ListRootPath) {
  settings_query query(1);
  query.list("/");
  const std::string request = query.request();
  EXPECT_FALSE(request.empty());
}

// ============================================================================
// Key Values Boundary Tests
// ============================================================================

class KeyValuesBoundaryTest : public ::testing::Test {};

TEST_F(KeyValuesBoundaryTest, VeryLongPath) {
  const std::string longPath(1000, 'a');  // 1000 character path
  const settings_query::key_values kv(longPath, "key", std::string("value"));
  EXPECT_EQ(longPath, kv.path());
}

TEST_F(KeyValuesBoundaryTest, VeryLongKey) {
  const std::string longKey(1000, 'k');  // 1000 character key
  const settings_query::key_values kv("/path", longKey, std::string("value"));
  EXPECT_EQ(longKey, kv.key());
}

TEST_F(KeyValuesBoundaryTest, VeryLongValue) {
  const std::string longValue(10000, 'v');  // 10000 character value
  const settings_query::key_values kv("/path", "key", longValue);
  EXPECT_EQ(longValue, kv.get_string());
}

TEST_F(KeyValuesBoundaryTest, MaxLongLongValue) {
  constexpr long long maxVal = 9223372036854775807LL;
  const settings_query::key_values kv("/path", "key", maxVal);
  EXPECT_EQ(maxVal, kv.get_int());
}

TEST_F(KeyValuesBoundaryTest, MinLongLongValue) {
  constexpr long long minVal = -9223372036854775807LL - 1;
  const settings_query::key_values kv("/path", "key", minVal);
  EXPECT_EQ(minVal, kv.get_int());
}

// ============================================================================
// Key Values with Null/Empty Edge Cases
// ============================================================================

class KeyValuesNullEdgeCaseTest : public ::testing::Test {};

TEST_F(KeyValuesNullEdgeCaseTest, AllEmptyStrings) {
  const settings_query::key_values kv("", "", std::string(""));
  EXPECT_EQ("", kv.path());
  EXPECT_EQ("", kv.key());
  EXPECT_EQ("", kv.get_string());
  EXPECT_EQ(0, kv.get_int());
  EXPECT_FALSE(kv.get_bool());
}

TEST_F(KeyValuesNullEdgeCaseTest, EmptyStringValue) {
  const settings_query::key_values kv("/path", "key", std::string(""));
  EXPECT_EQ("", kv.get_string());
}

TEST_F(KeyValuesNullEdgeCaseTest, WhitespaceOnlyValue) {
  const settings_query::key_values kv("/path", "key", std::string("   "));
  EXPECT_EQ("   ", kv.get_string());
}

TEST_F(KeyValuesNullEdgeCaseTest, NewlineInValue) {
  const settings_query::key_values kv("/path", "key", std::string("line1\nline2\nline3"));
  EXPECT_EQ("line1\nline2\nline3", kv.get_string());
}

TEST_F(KeyValuesNullEdgeCaseTest, TabInValue) {
  const settings_query::key_values kv("/path", "key", std::string("col1\tcol2\tcol3"));
  EXPECT_EQ("col1\tcol2\tcol3", kv.get_string());
}

// ============================================================================
// Multiple Queries Same Instance Tests
// ============================================================================

class MultipleQueriesSameInstanceTest : public ::testing::Test {};

TEST_F(MultipleQueriesSameInstanceTest, RequestAccumulatesOperations) {
  settings_query query(1);
  query.get("/path1", "key1", std::string("default1"));
  const std::string request1 = query.request();

  query.get("/path2", "key2", std::string("default2"));
  const std::string request2 = query.request();

  // Second request should be longer as it contains both operations
  EXPECT_GT(request2.length(), request1.length());
}

TEST_F(MultipleQueriesSameInstanceTest, MultipleSetsAccumulate) {
  settings_query query(1);
  query.set("/path1", "key1", "value1");
  const std::string request1 = query.request();

  query.set("/path2", "key2", "value2");
  query.set("/path3", "key3", "value3");
  const std::string request2 = query.request();

  EXPECT_GT(request2.length(), request1.length());
}
