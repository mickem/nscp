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
#include <str/nscp_string.hpp>
#include <string>
#include <vector>

// Tests for split2
TEST(nscp_string, split2_basic) {
  auto result = strEx::s::split2("key=value", "=");
  EXPECT_EQ(boost::get<0>(result), "key");
  EXPECT_EQ(boost::get<1>(result), "value");
}

TEST(nscp_string, split2_no_delimiter) {
  auto result = strEx::s::split2("keyvalue", "=");
  EXPECT_EQ(boost::get<0>(result), "keyvalue");
  EXPECT_EQ(boost::get<1>(result), "");
}

TEST(nscp_string, split2_delimiter_at_start) {
  auto result = strEx::s::split2("=value", "=");
  EXPECT_EQ(boost::get<0>(result), "");
  EXPECT_EQ(boost::get<1>(result), "value");
}

TEST(nscp_string, split2_delimiter_at_end) {
  auto result = strEx::s::split2("key=", "=");
  EXPECT_EQ(boost::get<0>(result), "key");
  EXPECT_EQ(boost::get<1>(result), "");
}

TEST(nscp_string, split2_empty_string) {
  auto result = strEx::s::split2("", "=");
  EXPECT_EQ(boost::get<0>(result), "");
  EXPECT_EQ(boost::get<1>(result), "");
}

TEST(nscp_string, split2_multiple_delimiters) {
  auto result = strEx::s::split2("key=val=ue", "=");
  EXPECT_EQ(boost::get<0>(result), "key");
  EXPECT_EQ(boost::get<1>(result), "val=ue");
}

// Tests for splitEx
TEST(nscp_string, splitEx_basic) {
  std::list<std::string> result = strEx::s::splitEx("a,b,c", ",");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "a");
  EXPECT_EQ(*it++, "b");
  EXPECT_EQ(*it++, "c");
}

TEST(nscp_string, splitEx_no_delimiter) {
  std::list<std::string> result = strEx::s::splitEx("abc", ",");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front(), "abc");
}

TEST(nscp_string, splitEx_empty_string) {
  std::list<std::string> result = strEx::s::splitEx("", ",");
  EXPECT_TRUE(result.empty());
}

TEST(nscp_string, splitEx_consecutive_delimiters) {
  std::list<std::string> result = strEx::s::splitEx("a,,c", ",");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "a");
  EXPECT_EQ(*it++, "");
  EXPECT_EQ(*it++, "c");
}

TEST(nscp_string, splitEx_delimiter_at_end) {
  std::list<std::string> result = strEx::s::splitEx("a,b,", ",");
  ASSERT_EQ(result.size(), 2u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "a");
  EXPECT_EQ(*it++, "b");
}

// Tests for split template
TEST(nscp_string, split_to_vector) {
  std::vector<std::string> result = strEx::s::split<std::vector<std::string>>("x|y|z", "|");
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "x");
  EXPECT_EQ(result[1], "y");
  EXPECT_EQ(result[2], "z");
}

TEST(nscp_string, split_to_list) {
  std::list<std::string> result = strEx::s::split<std::list<std::string>>("one:two:three", ":");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "one");
  EXPECT_EQ(*it++, "two");
  EXPECT_EQ(*it++, "three");
}

TEST(nscp_string, split_empty_string) {
  std::vector<std::string> result = strEx::s::split<std::vector<std::string>>("", ",");
  EXPECT_TRUE(result.empty());
}

// Tests for joinEx
TEST(nscp_string, joinEx_basic) {
  std::list<std::string> lst = {"a", "b", "c"};
  EXPECT_EQ(strEx::s::joinEx(lst, ", "), "a, b, c");
}

TEST(nscp_string, joinEx_empty_list) {
  std::list<std::string> lst;
  EXPECT_EQ(strEx::s::joinEx(lst, ", "), "");
}

TEST(nscp_string, joinEx_single_element) {
  std::list<std::string> lst = {"only"};
  EXPECT_EQ(strEx::s::joinEx(lst, ", "), "only");
}

TEST(nscp_string, joinEx_vector) {
  std::vector<std::string> vec = {"x", "y", "z"};
  EXPECT_EQ(strEx::s::joinEx(vec, "-"), "x-y-z");
}

TEST(nscp_string, joinEx_empty_separator) {
  std::list<std::string> lst = {"a", "b", "c"};
  EXPECT_EQ(strEx::s::joinEx(lst, ""), "abc");
}

// Tests for getToken
TEST(nscp_string, getToken_basic) {
  strEx::s::token result = strEx::s::getToken("key=value", '=');
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "value");
}

TEST(nscp_string, getToken_no_delimiter) {
  strEx::s::token result = strEx::s::getToken("keyvalue", '=');
  EXPECT_EQ(result.first, "keyvalue");
  EXPECT_EQ(result.second, "");
}

TEST(nscp_string, getToken_delimiter_at_start) {
  strEx::s::token result = strEx::s::getToken("=value", '=');
  EXPECT_EQ(result.first, "");
  EXPECT_EQ(result.second, "value");
}

TEST(nscp_string, getToken_delimiter_at_end) {
  strEx::s::token result = strEx::s::getToken("key=", '=');
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "");
}

TEST(nscp_string, getToken_empty_string) {
  strEx::s::token result = strEx::s::getToken("", '=');
  EXPECT_EQ(result.first, "");
  EXPECT_EQ(result.second, "");
}

TEST(nscp_string, getToken_multiple_delimiters) {
  strEx::s::token result = strEx::s::getToken("key=val=ue", '=');
  EXPECT_EQ(result.first, "key");
  EXPECT_EQ(result.second, "val=ue");
}

// Tests for parse_command (template version)
TEST(nscp_string, parse_command_template_basic) {
  std::list<std::string> args;
  strEx::s::parse_command("cmd arg1 arg2", args);
  ASSERT_EQ(args.size(), 3u);
  auto it = args.begin();
  EXPECT_EQ(*it++, "cmd");
  EXPECT_EQ(*it++, "arg1");
  EXPECT_EQ(*it++, "arg2");
}

TEST(nscp_string, parse_command_template_with_quotes) {
  std::list<std::string> args;
  strEx::s::parse_command("cmd \"arg with spaces\" arg2", args);
  ASSERT_EQ(args.size(), 3u);
  auto it = args.begin();
  EXPECT_EQ(*it++, "cmd");
  EXPECT_EQ(*it++, "arg with spaces");
  EXPECT_EQ(*it++, "arg2");
}

TEST(nscp_string, parse_command_template_empty) {
  std::list<std::string> args;
  strEx::s::parse_command("", args);
  EXPECT_TRUE(args.empty());
}

TEST(nscp_string, parse_command_template_to_vector) {
  std::vector<std::string> args;
  strEx::s::parse_command("a b c", args);
  ASSERT_EQ(args.size(), 3u);
  EXPECT_EQ(args[0], "a");
  EXPECT_EQ(args[1], "b");
  EXPECT_EQ(args[2], "c");
}

// Tests for parse_command (returning list version)
TEST(nscp_string, parse_command_return_basic) {
  std::list<std::string> result = strEx::s::parse_command("cmd arg1 arg2");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "cmd");
  EXPECT_EQ(*it++, "arg1");
  EXPECT_EQ(*it++, "arg2");
}

TEST(nscp_string, parse_command_return_with_quotes) {
  std::list<std::string> result = strEx::s::parse_command("cmd \"quoted arg\" arg2");
  ASSERT_EQ(result.size(), 3u);
  auto it = result.begin();
  EXPECT_EQ(*it++, "cmd");
  EXPECT_EQ(*it++, "quoted arg");
  EXPECT_EQ(*it++, "arg2");
}

TEST(nscp_string, parse_command_return_empty) {
  std::list<std::string> result = strEx::s::parse_command("");
  EXPECT_TRUE(result.empty());
}

TEST(nscp_string, parse_command_return_single) {
  std::list<std::string> result = strEx::s::parse_command("single");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front(), "single");
}

// Tests for rpad
TEST(nscp_string, rpad_basic) { EXPECT_EQ(strEx::s::rpad("abc", 5), "  abc"); }

TEST(nscp_string, rpad_exact_length) { EXPECT_EQ(strEx::s::rpad("abc", 3), "abc"); }

TEST(nscp_string, rpad_truncate) { EXPECT_EQ(strEx::s::rpad("abcdef", 3), "def"); }

TEST(nscp_string, rpad_empty_string) { EXPECT_EQ(strEx::s::rpad("", 3), "   "); }

TEST(nscp_string, rpad_length_one) { EXPECT_EQ(strEx::s::rpad("x", 1), "x"); }

TEST(nscp_string, rpad_zero_length) { EXPECT_EQ(strEx::s::rpad("abc", 0), ""); }

// Tests for lpad
TEST(nscp_string, lpad_basic) { EXPECT_EQ(strEx::s::lpad("abc", 5), "abc  "); }

TEST(nscp_string, lpad_exact_length) { EXPECT_EQ(strEx::s::lpad("abc", 3), "abc"); }

TEST(nscp_string, lpad_truncate) { EXPECT_EQ(strEx::s::lpad("abcdef", 3), "abc"); }

TEST(nscp_string, lpad_empty_string) { EXPECT_EQ(strEx::s::lpad("", 3), "   "); }

TEST(nscp_string, lpad_length_one) { EXPECT_EQ(strEx::s::lpad("x", 1), "x"); }

TEST(nscp_string, lpad_zero_length) { EXPECT_EQ(strEx::s::lpad("abc", 0), ""); }
