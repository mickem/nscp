#include <gtest/gtest.h>

#include <parsers/perfconfig/perfconfig.hpp>
#include <sstream>
#include <string>
#include <vector>

std::string to_string(const parsers::perfconfig::result_type &v) {
  std::stringstream ss;
  for (const parsers::perfconfig::perf_rule &r : v) {
    ss << r.name << "(";
    for (const parsers::perfconfig::perf_option &o : r.options) {
      ss << o.key << ":" << o.value << ";";
    }
    ss << ")";
  }
  return ss.str();
}
bool do_parse(const std::string& str, parsers::perfconfig::result_type &v) {
  v.clear();
  return parsers::perfconfig::parse(str, v);
}

parsers::perfconfig::result_type v;

// ==============================================================
// Basic parsing
// ==============================================================

TEST(PerfConfigTest, simple_string) {
  EXPECT_TRUE(do_parse("foo(a:b)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("foo(a:b;)", to_string(v));
}

TEST(PerfConfigTest, single_rule_fields) {
  EXPECT_TRUE(do_parse("foo(a:b)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("foo", v[0].name);
  ASSERT_EQ(1, v[0].options.size());
  EXPECT_EQ("a", v[0].options[0].key);
  EXPECT_EQ("b", v[0].options[0].value);
}

// ==============================================================
// Multiple rules
// ==============================================================

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

TEST(PerfConfigTest, different_rule_names) {
  EXPECT_TRUE(do_parse("alpha(x:1)beta(y:2)gamma(z:3)", v));
  ASSERT_EQ(3, v.size());
  EXPECT_EQ("alpha", v[0].name);
  EXPECT_EQ("beta", v[1].name);
  EXPECT_EQ("gamma", v[2].name);
}

// ==============================================================
// Multiple options
// ==============================================================

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

TEST(PerfConfigTest, option_key_only_single) {
  EXPECT_TRUE(do_parse("foo(key)", v));
  ASSERT_EQ(1, v.size());
  ASSERT_EQ(1, v[0].options.size());
  EXPECT_EQ("key", v[0].options[0].key);
  EXPECT_EQ("", v[0].options[0].value);
}

TEST(PerfConfigTest, multiple_key_only_options) {
  EXPECT_TRUE(do_parse("foo(a;b;c)", v));
  ASSERT_EQ(1, v.size());
  ASSERT_EQ(3, v[0].options.size());
  EXPECT_EQ("a", v[0].options[0].key);
  EXPECT_EQ("", v[0].options[0].value);
  EXPECT_EQ("b", v[0].options[1].key);
  EXPECT_EQ("", v[0].options[1].value);
  EXPECT_EQ("c", v[0].options[2].key);
  EXPECT_EQ("", v[0].options[2].value);
}

TEST(PerfConfigTest, mixed_key_only_and_key_value) {
  EXPECT_TRUE(do_parse("foo(a;b:val;c)", v));
  ASSERT_EQ(1, v.size());
  ASSERT_EQ(3, v[0].options.size());
  EXPECT_EQ("a", v[0].options[0].key);
  EXPECT_EQ("", v[0].options[0].value);
  EXPECT_EQ("b", v[0].options[1].key);
  EXPECT_EQ("val", v[0].options[1].value);
  EXPECT_EQ("c", v[0].options[2].key);
  EXPECT_EQ("", v[0].options[2].value);
}

// ==============================================================
// Whitespace handling
// ==============================================================

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

TEST(PerfConfigTest, spaces_around_semicolons) {
  EXPECT_TRUE(do_parse("foo( a : b ; c : d )", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("foo(a:b;c:d;)", to_string(v));
}

// ==============================================================
// Special characters in keywords
// ==============================================================

TEST(PerfConfigTest, simple_star_1) {
  EXPECT_TRUE(do_parse("*(a:b)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("*(a:b;)", to_string(v));
}

TEST(PerfConfigTest, hyphen_in_keyword) {
  EXPECT_TRUE(do_parse("my-rule(my-key:my-value)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("my-rule", v[0].name);
  EXPECT_EQ("my-key", v[0].options[0].key);
  EXPECT_EQ("my-value", v[0].options[0].value);
}

TEST(PerfConfigTest, underscore_in_keyword) {
  EXPECT_TRUE(do_parse("my_rule(my_key:my_value)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("my_rule", v[0].name);
  EXPECT_EQ("my_key", v[0].options[0].key);
  EXPECT_EQ("my_value", v[0].options[0].value);
}

TEST(PerfConfigTest, numeric_keyword) {
  EXPECT_TRUE(do_parse("123(456:789)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("123", v[0].name);
  EXPECT_EQ("456", v[0].options[0].key);
  EXPECT_EQ("789", v[0].options[0].value);
}

TEST(PerfConfigTest, plus_sign_in_keyword) {
  EXPECT_TRUE(do_parse("foo+(a+:b+)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("foo+", v[0].name);
  EXPECT_EQ("a+", v[0].options[0].key);
  EXPECT_EQ("b+", v[0].options[0].value);
}

TEST(PerfConfigTest, dot_in_keyword) {
  EXPECT_TRUE(do_parse("foo.bar(a.b:c.d)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("foo.bar", v[0].name);
  EXPECT_EQ("a.b", v[0].options[0].key);
  EXPECT_EQ("c.d", v[0].options[0].value);
}

TEST(PerfConfigTest, wildcard_rule_name) {
  EXPECT_TRUE(do_parse("*(suffix:g)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("*", v[0].name);
  EXPECT_EQ("suffix", v[0].options[0].key);
  EXPECT_EQ("g", v[0].options[0].value);
}

// ==============================================================
// Advanced / combined
// ==============================================================

TEST(PerfConfigTest, simple_multiple_advanced) {
  EXPECT_TRUE(do_parse("foo(a:b)foo(1:b)    bar(e:r) test(a:b;e:r;f:t)", v));
  ASSERT_EQ(4, v.size());
  EXPECT_EQ("foo(a:b;)foo(1:b;)bar(e:r;)test(a:b;e:r;f:t;)", to_string(v));
}

TEST(PerfConfigTest, multiple_rules_with_multiple_options) {
  EXPECT_TRUE(do_parse("rule1(a:1;b:2) rule2(c:3;d:4;e:5)", v));
  ASSERT_EQ(2, v.size());
  ASSERT_EQ(2, v[0].options.size());
  ASSERT_EQ(3, v[1].options.size());
  EXPECT_EQ("rule1(a:1;b:2;)rule2(c:3;d:4;e:5;)", to_string(v));
}

// ==============================================================
// Empty / invalid input
// ==============================================================

TEST(PerfConfigTest, empty_string) {
  EXPECT_TRUE(do_parse("", v));
  EXPECT_EQ(0, v.size());
}

TEST(PerfConfigTest, whitespace_only) {
  EXPECT_TRUE(do_parse("   ", v));
  EXPECT_EQ(0, v.size());
}

TEST(PerfConfigTest, missing_closing_paren) {
  EXPECT_FALSE(do_parse("foo(a:b", v));
}

TEST(PerfConfigTest, missing_opening_paren) {
  EXPECT_FALSE(do_parse("foo a:b)", v));
}

TEST(PerfConfigTest, no_options) {
  EXPECT_TRUE(do_parse("foo(a)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("foo", v[0].name);
  ASSERT_EQ(1, v[0].options.size());
  EXPECT_EQ("a", v[0].options[0].key);
  EXPECT_EQ("", v[0].options[0].value);
}

// ==============================================================
// Realistic use cases
// ==============================================================

TEST(PerfConfigTest, realistic_perf_config) {
  EXPECT_TRUE(do_parse("*(suffix:g)CPU(unit:ms)", v));
  ASSERT_EQ(2, v.size());
  EXPECT_EQ("*", v[0].name);
  EXPECT_EQ("suffix", v[0].options[0].key);
  EXPECT_EQ("g", v[0].options[0].value);
  EXPECT_EQ("CPU", v[1].name);
  EXPECT_EQ("unit", v[1].options[0].key);
  EXPECT_EQ("ms", v[1].options[0].value);
}

TEST(PerfConfigTest, realistic_multiple_config) {
  EXPECT_TRUE(do_parse("*(suffix:g;prefix:check)disk(unit:B)", v));
  ASSERT_EQ(2, v.size());
  EXPECT_EQ("*(suffix:g;prefix:check;)disk(unit:B;)", to_string(v));
}

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

// ==============================================================
// Quoted values
// ==============================================================

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

TEST(PerfConfigTest, quoted_value_with_special_chars) {
  EXPECT_TRUE(do_parse("foo(a:'hello world')", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("a", v[0].options[0].key);
  EXPECT_EQ("hello world", v[0].options[0].value);
}

TEST(PerfConfigTest, quoted_value_with_semicolons) {
  EXPECT_TRUE(do_parse("foo(a:'val;with;semis')", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("a", v[0].options[0].key);
  EXPECT_EQ("val;with;semis", v[0].options[0].value);
}

TEST(PerfConfigTest, quoted_value_with_colons) {
  EXPECT_TRUE(do_parse("foo(a:'val:with:colons')", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("a", v[0].options[0].key);
  EXPECT_EQ("val:with:colons", v[0].options[0].value);
}

TEST(PerfConfigTest, quoted_value_with_parens) {
  EXPECT_TRUE(do_parse("foo(a:'val(with)parens')", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("a", v[0].options[0].key);
  EXPECT_EQ("val(with)parens", v[0].options[0].value);
}

TEST(PerfConfigTest, quoted_and_unquoted_mixed) {
  EXPECT_TRUE(do_parse("foo(a:'quoted value';b:plain)", v));
  ASSERT_EQ(1, v.size());
  ASSERT_EQ(2, v[0].options.size());
  EXPECT_EQ("quoted value", v[0].options[0].value);
  EXPECT_EQ("plain", v[0].options[1].value);
}

TEST(PerfConfigTest, keyword_with_spaces_in_name) {
  EXPECT_TRUE(do_parse("foo bar(a:b)", v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ("foo bar", v[0].name);
}
