#include <vector>
#include <string>
#include <nscapi/functions.hpp>

#include <gtest/gtest.h>

std::string do_parse(std::string str) {
	Plugin::QueryResponseMessage::Response r;
	nscapi::functions::parse_performance_data(&r, str);
	return nscapi::functions::build_performance_data(r);
}

TEST(PerfDataTest, empty_string) {
	EXPECT_EQ("", do_parse(""));
}

TEST(PerfDataTest, full_string) {
	EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("aaa=1g;0;4;2;5"));
}
TEST(PerfDataTest, full_string_with_ticks) {
	EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("aaa=1g;0;4;2;5"));
}
/*
TEST(PerfDataTest, full_spaces) {
	EXPECT_EQ("'aaa'=1g;0;4;2;5", do_parse("     'aaa'=1g;0;4;2;5     "));
}
*/
TEST(PerfDataTest, mltiple_strings) {
	EXPECT_EQ("'aaa'=1g;0;4;2;5 'bbb'=2g;3;4;2;5", do_parse("aaa=1g;0;4;2;5 bbb=2g;3;4;2;5"));
}
TEST(PerfDataTest, only_value) {
	EXPECT_EQ("'aaa'=1g", do_parse("aaa=1g"));
}
TEST(PerfDataTest, value_with_warncrit) {
	EXPECT_EQ("'aaa'=1g;0;5", do_parse("aaa=1g;0;5"));
}
TEST(PerfDataTest, value_without_warncrit_with_maxmin) {
	EXPECT_EQ("'aaa'=1g;;;0;5", do_parse("aaa=1g;;;0;5"));
}
TEST(PerfDataTest, value_without_warncrit_maxmin) {
	EXPECT_EQ("'aaa'=1g", do_parse("aaa=1g"));
}

TEST(PerfDataTest, value_various_reparse) {
	std::vector<std::string> strings;
	strings.push_back("'aaa'=1g;0;4;2;5");
	strings.push_back("'aaa'=6g;1;2;3;4");
	strings.push_back("'aaa'=6g;;2;3;4");
	strings.push_back("'aaa'=6g;1;;3;4");
	strings.push_back("'aaa'=6g;1;2;;4");
	strings.push_back("'aaa'=6g;1;2;3");
	strings.push_back("'aaa'=6g;;;3;4");
	strings.push_back("'aaa'=6g;1;;;4");
	strings.push_back("'aaa'=6g;1;2");
	strings.push_back("'aaa'=6g");
	BOOST_FOREACH(std::string s, strings) {
		EXPECT_EQ(s.c_str(), do_parse(s));
	}
}
