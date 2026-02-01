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

#include <list>
#include <str/utils.hpp>
#include <string>
#include <vector>

// ============================================================================
// Tests for replace (from utils_no_boost.hpp)
// ============================================================================
TEST(str_utils, replace_basic) {
  std::string str = "hello world";
  str::utils::replace(str, "world", "universe");
  EXPECT_EQ(str, "hello universe");
}

TEST(str_utils, replace_multiple_occurrences) {
  std::string str = "aaa";
  str::utils::replace(str, "a", "b");
  EXPECT_EQ(str, "bbb");
}

TEST(str_utils, replace_not_found) {
  std::string str = "hello world";
  str::utils::replace(str, "xyz", "abc");
  EXPECT_EQ(str, "hello world");
}

TEST(str_utils, replace_empty_string) {
  std::string str = "";
  str::utils::replace(str, "a", "b");
  EXPECT_EQ(str, "");
}

TEST(str_utils, replace_with_empty) {
  std::string str = "hello world";
  str::utils::replace(str, "world", "");
  EXPECT_EQ(str, "hello ");
}

TEST(str_utils, replace_with_longer) {
  std::string str = "ab";
  str::utils::replace(str, "a", "aaa");
  EXPECT_EQ(str, "aaab");
}

TEST(str_utils, replace_overlapping) {
  std::string str = "aaa";
  str::utils::replace(str, "aa", "b");
  EXPECT_EQ(str, "ba");
}

// ============================================================================
// Tests for split (template, out parameter version from utils_no_boost.hpp)
// ============================================================================
TEST(str_utils, split_outparam_basic) {
  std::vector<std::string> result;
  str::utils::split(result, "a,b,c", ",");
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
  EXPECT_EQ(result[2], "c");
}

TEST(str_utils, split_outparam_no_delimiter) {
  std::vector<std::string> result;
  str::utils::split(result, "abc", ",");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "abc");
}

TEST(str_utils, split_outparam_empty_string) {
  std::vector<std::string> result;
  str::utils::split(result, "", ",");
  EXPECT_TRUE(result.empty());
}

TEST(str_utils, split_outparam_consecutive_delimiters) {
  std::list<std::string> result;
  str::utils::split(result, "a,,c", ",");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "a");
  EXPECT_EQ(*it++, "");
  EXPECT_EQ(*it++, "c");
}

// ============================================================================
// Tests for split2 (from utils_no_boost.hpp)
// ============================================================================
TEST(str_utils, split2_basic) {
  auto result = str::utils::split2("key=value", "=");
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "value");
}

TEST(str_utils, split2_no_delimiter) {
  auto result = str::utils::split2("keyvalue", "=");
  EXPECT_EQ(result.first, "keyvalue");
  EXPECT_EQ(result.second, "");
}

TEST(str_utils, split2_delimiter_at_start) {
  auto result = str::utils::split2("=value", "=");
  EXPECT_EQ(result.first, "");
  EXPECT_EQ(result.second, "value");
}

TEST(str_utils, split2_delimiter_at_end) {
  auto result = str::utils::split2("key=", "=");
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "");
}

TEST(str_utils, split2_empty_string) {
  auto result = str::utils::split2("", "=");
  EXPECT_EQ(result.first, "");
  EXPECT_EQ(result.second, "");
}

TEST(str_utils, split2_multiple_delimiters) {
  auto result = str::utils::split2("key=val=ue", "=");
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "val=ue");
}

TEST(str_utils, split2_multi_char_delimiter) {
  auto result = str::utils::split2("key::value", "::");
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "value");
}

// ============================================================================
// Tests for split_lst (from utils_no_boost.hpp)
// ============================================================================
TEST(str_utils, split_lst_basic) {
  std::list<std::string> result = str::utils::split_lst("a,b,c", ",");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "a");
  EXPECT_EQ(*it++, "b");
  EXPECT_EQ(*it++, "c");
}

TEST(str_utils, split_lst_no_delimiter) {
  std::list<std::string> result = str::utils::split_lst("abc", ",");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front(), "abc");
}

TEST(str_utils, split_lst_empty_string) {
  std::list<std::string> result = str::utils::split_lst("", ",");
  EXPECT_TRUE(result.empty());
}

TEST(str_utils, split_lst_delimiter_at_end) {
  std::list<std::string> result = str::utils::split_lst("a,b,", ",");
  ASSERT_EQ(result.size(), 2u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "a");
  EXPECT_EQ(*it++, "b");
}

// ============================================================================
// Tests for split (template, return version from utils_no_boost.hpp)
// ============================================================================
TEST(str_utils, split_template_to_vector) {
  std::vector<std::string> result = str::utils::split<std::vector<std::string>>("x|y|z", "|");
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "x");
  EXPECT_EQ(result[1], "y");
  EXPECT_EQ(result[2], "z");
}

TEST(str_utils, split_template_to_list) {
  std::list<std::string> result = str::utils::split<std::list<std::string>>("one:two:three", ":");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "one");
  EXPECT_EQ(*it++, "two");
  EXPECT_EQ(*it++, "three");
}

TEST(str_utils, split_template_empty_string) {
  std::vector<std::string> result = str::utils::split<std::vector<std::string>>("", ",");
  EXPECT_TRUE(result.empty());
}

// ============================================================================
// Tests for getToken (from utils_no_boost.hpp)
// ============================================================================
TEST(str_utils, getToken_basic) {
  str::utils::token result = str::utils::getToken("key=value", '=');
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "value");
}

TEST(str_utils, getToken_no_delimiter) {
  str::utils::token result = str::utils::getToken("keyvalue", '=');
  EXPECT_EQ(result.first, "keyvalue");
  EXPECT_EQ(result.second, "");
}

TEST(str_utils, getToken_delimiter_at_start) {
  str::utils::token result = str::utils::getToken("=value", '=');
  EXPECT_EQ(result.first, "");
  EXPECT_EQ(result.second, "value");
}

TEST(str_utils, getToken_delimiter_at_end) {
  str::utils::token result = str::utils::getToken("key=", '=');
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "");
}

TEST(str_utils, getToken_empty_string) {
  str::utils::token result = str::utils::getToken("", '=');
  EXPECT_EQ(result.first, "");
  EXPECT_EQ(result.second, "");
}

TEST(str_utils, getToken_multiple_delimiters) {
  str::utils::token result = str::utils::getToken("key=val=ue", '=');
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "val=ue");
}

// ============================================================================
// Tests for joinEx (from utils.hpp)
// ============================================================================
TEST(str_utils, joinEx_basic) {
  std::list<std::string> lst = {"a", "b", "c"};
  EXPECT_EQ(str::utils::joinEx(lst, ", "), "a, b, c");
}

TEST(str_utils, joinEx_empty_list) {
  std::list<std::string> lst;
  EXPECT_EQ(str::utils::joinEx(lst, ", "), "");
}

TEST(str_utils, joinEx_single_element) {
  std::list<std::string> lst = {"only"};
  EXPECT_EQ(str::utils::joinEx(lst, ", "), "only");
}

TEST(str_utils, joinEx_vector) {
  std::vector<std::string> vec = {"x", "y", "z"};
  EXPECT_EQ(str::utils::joinEx(vec, "-"), "x-y-z");
}

TEST(str_utils, joinEx_empty_separator) {
  std::list<std::string> lst = {"a", "b", "c"};
  EXPECT_EQ(str::utils::joinEx(lst, ""), "abc");
}

// ============================================================================
// Tests for parse_command (template version from utils.hpp)
// ============================================================================
TEST(str_utils, parse_command_template_basic) {
  std::list<std::string> args;
  str::utils::parse_command("cmd arg1 arg2", args);
  ASSERT_EQ(args.size(), 3u);
  auto it = args.begin();
  EXPECT_EQ(*it++, "cmd");
  EXPECT_EQ(*it++, "arg1");
  EXPECT_EQ(*it++, "arg2");
}

TEST(str_utils, parse_command_template_with_quotes) {
  std::list<std::string> args;
  str::utils::parse_command("cmd \"arg with spaces\" arg2", args);
  ASSERT_EQ(args.size(), 3u);
  auto it = args.begin();
  EXPECT_EQ(*it++, "cmd");
  EXPECT_EQ(*it++, "arg with spaces");
  EXPECT_EQ(*it++, "arg2");
}

TEST(str_utils, parse_command_template_empty) {
  std::list<std::string> args;
  str::utils::parse_command("", args);
  EXPECT_TRUE(args.empty());
}

TEST(str_utils, parse_command_template_to_vector) {
  std::vector<std::string> args;
  str::utils::parse_command("a b c", args);
  ASSERT_EQ(args.size(), 3u);
  EXPECT_EQ(args[0], "a");
  EXPECT_EQ(args[1], "b");
  EXPECT_EQ(args[2], "c");
}

TEST(str_utils, parse_command_template_multiple_spaces) {
  std::vector<std::string> args;
  str::utils::parse_command("a   b   c", args);
  ASSERT_EQ(args.size(), 3u);
  EXPECT_EQ(args[0], "a");
  EXPECT_EQ(args[1], "b");
  EXPECT_EQ(args[2], "c");
}

// ============================================================================
// Tests for parse_command (returning list version from utils.hpp)
// ============================================================================
TEST(str_utils, parse_command_return_basic) {
  std::list<std::string> result = str::utils::parse_command("cmd arg1 arg2");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "cmd");
  EXPECT_EQ(*it++, "arg1");
  EXPECT_EQ(*it++, "arg2");
}

TEST(str_utils, parse_command_return_with_quotes) {
  std::list<std::string> result = str::utils::parse_command("cmd \"quoted arg\" arg2");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "cmd");
  EXPECT_EQ(*it++, "quoted arg");
  EXPECT_EQ(*it++, "arg2");
}

TEST(str_utils, parse_command_return_empty) {
  std::list<std::string> result = str::utils::parse_command("");
  EXPECT_TRUE(result.empty());
}

TEST(str_utils, parse_command_return_single) {
  std::list<std::string> result = str::utils::parse_command("single");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front(), "single");
}

// ============================================================================
// Tests for parse_command (cmd + args version from utils.hpp)
// ============================================================================
TEST(str_utils, parse_command_cmd_args_basic) {
  std::string cmd;
  std::list<std::string> args;
  str::utils::parse_command("mycmd arg1 arg2", cmd, args);
  EXPECT_EQ(cmd, "mycmd");
  ASSERT_EQ(args.size(), 2u);
  auto it = args.begin();
  EXPECT_EQ(*it++, "arg1");
  EXPECT_EQ(*it++, "arg2");
}

TEST(str_utils, parse_command_cmd_args_with_quotes) {
  std::string cmd;
  std::list<std::string> args;
  str::utils::parse_command("mycmd \"arg with spaces\" arg2", cmd, args);
  EXPECT_EQ(cmd, "mycmd");
  ASSERT_EQ(args.size(), 2u);
  auto it = args.begin();
  EXPECT_EQ(*it++, "arg with spaces");
  EXPECT_EQ(*it++, "arg2");
}

TEST(str_utils, parse_command_cmd_args_empty) {
  std::string cmd;
  std::list<std::string> args;
  str::utils::parse_command("", cmd, args);
  EXPECT_EQ(cmd, "");
  EXPECT_TRUE(args.empty());
}

TEST(str_utils, parse_command_cmd_args_only_cmd) {
  std::string cmd;
  std::list<std::string> args;
  str::utils::parse_command("onlycmd", cmd, args);
  EXPECT_EQ(cmd, "onlycmd");
  EXPECT_TRUE(args.empty());
}

TEST(str_utils, parse_command_cmd_args_multiple_spaces) {
  std::string cmd;
  std::list<std::string> args;
  str::utils::parse_command("cmd   arg1   arg2", cmd, args);
  EXPECT_EQ(cmd, "cmd");
  ASSERT_EQ(args.size(), 2u);
  auto it = args.begin();
  EXPECT_EQ(*it++, "arg1");
  EXPECT_EQ(*it++, "arg2");
}
