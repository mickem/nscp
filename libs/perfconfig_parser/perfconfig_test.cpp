#include <vector>
#include <string>
#include <sstream>

#include <parsers/perfconfig/perfconfig.hpp>

#include <gtest/gtest.h>
#include <boost/foreach.hpp>
#include <boost/version.hpp>

std::string to_string(const parsers::perfconfig::result_type &v) {
	std::stringstream ss;
	BOOST_FOREACH(const parsers::perfconfig::perf_rule &r, v) {
		ss << r.name << "(";
		BOOST_FOREACH(const parsers::perfconfig::perf_option &o, r.options) {
			ss << o.key << ":" << o.value << ";";
		}
		ss << ")";
	}
	return ss.str();
}
bool do_parse(std::string str, parsers::perfconfig::result_type &v) {
	v.clear();
	parsers::perfconfig expr;
	return expr.parse(str, v);
}

parsers::perfconfig::result_type v;

#if BOOST_VERSION >= 104200

TEST(PerfConfigTest, simple_string) {
	EXPECT_TRUE(do_parse("foo(a:b)", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("foo(a:b;)", to_string(v));
}

TEST(PerfConfigTest, simple_multiple_rules_1) {
	EXPECT_TRUE(do_parse("foo(a:b)foo(1:b)", v));
	ASSERT_EQ(2, v.size());
	EXPECT_EQ("foo(a:b;)foo(1:b;)", to_string(v));
}
TEST(PerfConfigTest, simple_multiple_rules_2) {
	EXPECT_TRUE(do_parse("foo(a:b)foo(1:b)foo(1:b)foo(1:b)foo(1:b)foo(1:b)foo(1:b)foo(1:b)", v));
	ASSERT_EQ(8, v.size());
	EXPECT_EQ("foo(a:b;)foo(1:b;)foo(1:b;)foo(1:b;)foo(1:b;)foo(1:b;)foo(1:b;)foo(1:b;)", to_string(v));
}
TEST(PerfConfigTest, simple_multiple_opts_1) {
	EXPECT_TRUE(do_parse("foo(a:b;1:4)", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("foo(a:b;1:4;)", to_string(v));
}
TEST(PerfConfigTest, simple_multiple_opts_2) {
	EXPECT_TRUE(do_parse("foo(a:b;1:4;1:4;r:5;e:yui;qwer:r;qwerf45:errtfv)", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("foo(a:b;1:4;1:4;r:5;e:yui;qwer:r;qwerf45:errtfv;)", to_string(v));
}

TEST(PerfConfigTest, simple_multiple_opts_3) {
	EXPECT_TRUE(do_parse("foo(a;a:b)", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("foo(a:;a:b;)", to_string(v));
}

TEST(PerfConfigTest, simple_space_1) {
	EXPECT_TRUE(do_parse("   foo(a:b)   foo(1:b)   ", v));
	ASSERT_EQ(2, v.size());
	EXPECT_EQ("foo(a:b;)foo(1:b;)", to_string(v));
}
TEST(PerfConfigTest, simple_space_2) {
	EXPECT_TRUE(do_parse("   foo  (a:b)   foo  (1:b)   ", v));
	ASSERT_EQ(2, v.size());
	EXPECT_EQ("foo(a:b;)foo(1:b;)", to_string(v));
}
TEST(PerfConfigTest, simple_space_3) {
	EXPECT_TRUE(do_parse("foo(  a:  b)", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("foo(a:b;)", to_string(v));
}
TEST(PerfConfigTest, simple_space_4) {
	EXPECT_TRUE(do_parse("foo(  a  :  b  )", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("foo(a:b;)", to_string(v));
}
#if BOOST_VERSION >= 104900
// These test only works in boost after 1.49
TEST(PerfConfigTest, percentage_sign) {
	EXPECT_TRUE(do_parse("foo %(a:b)", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("foo %(a:b;)", to_string(v));
}
TEST(PerfConfigTest, simple_space_5) {
	EXPECT_TRUE(do_parse("foo(  a  b :  b  c )", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("foo(a  b:b  c;)", to_string(v));
}
TEST(PerfConfigTest, simple_space_6) {
	EXPECT_TRUE(do_parse("   foo(  a  :  b  ;  12   : h  h    h )   foo(  a  :  b    k k  )   foo(  a  :  b  )  ", v));
	ASSERT_EQ(3, v.size());
	EXPECT_EQ("foo(a:b;12:h  h    h;)foo(a:b    k k;)foo(a:b;)", to_string(v));
}
TEST(PerfConfigTest, simple_qoutes) {
	EXPECT_TRUE(do_parse("foo(a:'b')foo(a:'b    k k')foo(  a  :  'b'; c:'d'  )  ", v));
	ASSERT_EQ(3, v.size());
	EXPECT_EQ("foo(a:b;)foo(a:b    k k;)foo(a:b;c:d;)", to_string(v));
}
TEST(PerfConfigTest, simple_empty_qoutes) {
	EXPECT_TRUE(do_parse("foo(a:'')foo(a:'b    k k')foo(  a  :  'b'; c:''  )  ", v));
	ASSERT_EQ(3, v.size());
	EXPECT_EQ("foo(a:;)foo(a:b    k k;)foo(a:b;c:;)", to_string(v));
}
#endif
TEST(PerfConfigTest, simple_star_1) {
	EXPECT_TRUE(do_parse("*(a:b)", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ("*(a:b;)", to_string(v));
}

TEST(PerfConfigTest, simple_multiple_advanced) {
	EXPECT_TRUE(do_parse("foo(a:b)foo(1:b)    bar(e:r) test(a:b;e:r;f:t)", v));
	ASSERT_EQ(4, v.size());
	EXPECT_EQ("foo(a:b;)foo(1:b;)bar(e:r;)test(a:b;e:r;f:t;)", to_string(v));
}

#endif