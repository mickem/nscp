#include <vector>
#include <string>
#include <parsers/expression/expression.hpp>

#include <gtest/gtest.h>

bool do_parse(std::string str, parsers::simple_expression::result_type &v) {
	v.clear();
	parsers::simple_expression expr;
	return expr.parse(str, v);
}

parsers::simple_expression::result_type v;

TEST(ExpressionTest, simple_string) {
	EXPECT_TRUE(do_parse("HelloWorld", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ(false, v[0].is_variable);
	EXPECT_EQ("HelloWorld", v[0].name);
}

TEST(ExpressionTest, simple_string_with_space) {
	EXPECT_TRUE(do_parse("Hello World", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ(false, v[0].is_variable);
	EXPECT_EQ("Hello World", v[0].name);
}

TEST(ExpressionTest, simple_string_with_chars) {
	EXPECT_TRUE(do_parse("Hello$}World}$Foo}Bar$End", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ(false, v[0].is_variable);
	EXPECT_EQ("Hello$}World}$Foo}Bar$End", v[0].name);
}

TEST(ExpressionTest, simple_expression) {
	EXPECT_TRUE(do_parse("${foobar}", v));
	ASSERT_EQ(1, v.size());
	EXPECT_EQ(true, v[0].is_variable);
	EXPECT_EQ("foobar", v[0].name);
}

TEST(ExpressionTest, simple_string_with_expression_1) {
	EXPECT_TRUE(do_parse("HelloWorld${foobar}", v));
	ASSERT_EQ(2, v.size());
	EXPECT_EQ(false, v[0].is_variable);
	EXPECT_EQ("HelloWorld", v[0].name);
	EXPECT_EQ(true, v[1].is_variable);
	EXPECT_EQ("foobar", v[1].name);
}

TEST(ExpressionTest, simple_string_with_expression_2) {
	EXPECT_TRUE(do_parse("HelloWorld${foobar}END", v));
	ASSERT_EQ(3, v.size());
	EXPECT_EQ(false, v[0].is_variable);
	EXPECT_EQ("HelloWorld", v[0].name);
	EXPECT_EQ(true, v[1].is_variable);
	EXPECT_EQ("foobar", v[1].name);
	EXPECT_EQ(false, v[2].is_variable);
	EXPECT_EQ("END", v[2].name);
}

TEST(ExpressionTest, complext_string_with_expressions) {
	EXPECT_TRUE(do_parse("HelloWorld${foobar}MoreData${test}-}}{{-${test{{2}", v));
	ASSERT_EQ(6, v.size());
	EXPECT_EQ("HelloWorld", v[0].name);
	EXPECT_EQ(false, v[0].is_variable);
	EXPECT_EQ("foobar", v[1].name);
	EXPECT_EQ(true, v[1].is_variable);
	EXPECT_EQ("MoreData", v[2].name);
	EXPECT_EQ(false, v[2].is_variable);
	EXPECT_EQ("test", v[3].name);
	EXPECT_EQ(true, v[3].is_variable);
	EXPECT_EQ("-}}{{-", v[4].name);
	EXPECT_EQ(false, v[4].is_variable);
	EXPECT_EQ("test{{2", v[5].name);
	EXPECT_EQ(true, v[5].is_variable);
}
