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

#include <map>
#include <parsers/where/node.hpp>
#include <parsers/where/variable.hpp>
#include <string>

using namespace parsers::where;

// Minimal object_factory used only for `get_performance_config_key` lookups.
// All other interface methods return defaults — the perf generator only ever
// calls `get_performance_config_key` during configure().
struct stub_perf_factory final : object_factory_interface {
  // Map of "object_key" -> { "config_key" -> "value" }, populated by tests.
  // The lookup mirrors the resolution order used by the real factory:
  // prefix.object.suffix → prefix.object → object.suffix → prefix → suffix → object → "*".
  std::map<std::string, std::map<std::string, std::string>> options;

  bool has_error() const override { return false; }
  std::string get_error() const override { return ""; }
  void error(std::string) override {}
  bool has_warn() const override { return false; }
  std::string get_warn() const override { return ""; }
  void warn(std::string) override {}
  void clear() override {}
  void enable_debug(bool) override {}
  bool debug_enabled() override { return false; }
  std::string get_debug() const override { return ""; }
  void debug(object_match) override {}

  bool can_convert(value_type, value_type) override { return false; }
  bool can_convert(std::string, std::shared_ptr<any_node>, value_type) override { return false; }
  std::shared_ptr<binary_function_impl> create_converter(std::string, std::shared_ptr<any_node>, value_type) override { return nullptr; }

  bool has_variable(const std::string &) override { return false; }
  node_type create_variable(const std::string &, bool) override { return nullptr; }
  bool has_function(const std::string &) override { return false; }
  node_type create_function(const std::string &, node_type) override { return nullptr; }

  bool lookup(const std::string &obj, const std::string &key, std::string &out) const {
    auto it = options.find(obj);
    if (it == options.end()) return false;
    auto it2 = it->second.find(key);
    if (it2 == it->second.end()) return false;
    out = it2->second;
    return true;
  }

  std::string get_performance_config_key(std::string prefix, std::string obj, std::string suffix, std::string key, std::string fallback) const override {
    std::string value = fallback;
    const bool has_p = !prefix.empty();
    const bool has_s = !suffix.empty();
    if (has_p && has_s && lookup(prefix + "." + obj + "." + suffix, key, value)) return value;
    if (has_p && lookup(prefix + "." + obj, key, value)) return value;
    if (has_s && lookup(obj + "." + suffix, key, value)) return value;
    if (has_p && lookup(prefix, key, value)) return value;
    if (has_s && lookup(suffix, key, value)) return value;
    if (lookup(obj, key, value)) return value;
    if (lookup("*", key, value)) return value;
    return value;
  }

  void set_option(const std::string &obj, const std::string &key, const std::string &value) { options[obj][key] = value; }
};

static object_factory make_factory() { return std::make_shared<stub_perf_factory>(); }
static stub_perf_factory *native(const object_factory &f) { return reinterpret_cast<stub_perf_factory *>(f.get()); }

// Each test just needs a generator + factory + (unused) evaluation_context +
// (unused) object. The generator only consults the factory during configure()
// and the value/warn/crit ints during eval().
struct dummy_object {};
typedef simple_number_performance_generator<dummy_object, long long> int_perf_gen;

namespace {

// =================================================================
// parse_optional_perf_bound — input parsing
// =================================================================

TEST(ParseOptionalPerfBound, AcceptsIntegers) {
  EXPECT_DOUBLE_EQ(*parse_optional_perf_bound("0"), 0.0);
  EXPECT_DOUBLE_EQ(*parse_optional_perf_bound("12345"), 12345.0);
  EXPECT_DOUBLE_EQ(*parse_optional_perf_bound("-5"), -5.0);
}

TEST(ParseOptionalPerfBound, AcceptsFloats) {
  EXPECT_DOUBLE_EQ(*parse_optional_perf_bound("3.14"), 3.14);
  EXPECT_DOUBLE_EQ(*parse_optional_perf_bound("0.5"), 0.5);
}

TEST(ParseOptionalPerfBound, AcceptsSurroundingWhitespace) { EXPECT_DOUBLE_EQ(*parse_optional_perf_bound("  42  "), 42.0); }

TEST(ParseOptionalPerfBound, EmptyReturnsNone) { EXPECT_FALSE(parse_optional_perf_bound("").is_initialized()); }

TEST(ParseOptionalPerfBound, RejectsGarbage) {
  EXPECT_FALSE(parse_optional_perf_bound("abc").is_initialized());
  EXPECT_FALSE(parse_optional_perf_bound("12abc").is_initialized());
}

// =================================================================
// simple_number_performance_generator — eval without overrides
// =================================================================

TEST(SimplePerfGenerator, EmitsValueWarnCritWithNoConfig) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 50, 80, 90, dummy_object{});

  ASSERT_EQ(list.size(), 1u);
  const auto &p = list.front();
  ASSERT_TRUE(p.float_value.is_initialized());
  EXPECT_DOUBLE_EQ(p.float_value->value, 50.0);
  ASSERT_TRUE(p.float_value->warn.is_initialized());
  EXPECT_DOUBLE_EQ(*p.float_value->warn, 80.0);
  ASSERT_TRUE(p.float_value->crit.is_initialized());
  EXPECT_DOUBLE_EQ(*p.float_value->crit, 90.0);
  EXPECT_FALSE(p.float_value->minimum.is_initialized());
  EXPECT_FALSE(p.float_value->maximum.is_initialized());
}

TEST(SimplePerfGenerator, IgnoredSuppressesOutput) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "ignored", "true");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 1, 2, 3, dummy_object{});
  EXPECT_TRUE(list.empty());
}

// =================================================================
// minimum / maximum from perf-config
// =================================================================

TEST(SimplePerfGenerator, MinimumFromPerfConfig) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "minimum", "0");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 50, 80, 90, dummy_object{});

  ASSERT_EQ(list.size(), 1u);
  ASSERT_TRUE(list.front().float_value->minimum.is_initialized());
  EXPECT_DOUBLE_EQ(*list.front().float_value->minimum, 0.0);
  EXPECT_FALSE(list.front().float_value->maximum.is_initialized());
}

TEST(SimplePerfGenerator, MaximumFromPerfConfig) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "maximum", "12345");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 50, 80, 90, dummy_object{});

  ASSERT_EQ(list.size(), 1u);
  ASSERT_TRUE(list.front().float_value->maximum.is_initialized());
  EXPECT_DOUBLE_EQ(*list.front().float_value->maximum, 12345.0);
  EXPECT_FALSE(list.front().float_value->minimum.is_initialized());
}

TEST(SimplePerfGenerator, BothBoundsFromPerfConfig) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "minimum", "0");
  native(factory)->set_option("counter", "maximum", "100");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 25, 50, 75, dummy_object{});

  ASSERT_EQ(list.size(), 1u);
  EXPECT_DOUBLE_EQ(*list.front().float_value->minimum, 0.0);
  EXPECT_DOUBLE_EQ(*list.front().float_value->maximum, 100.0);
}

TEST(SimplePerfGenerator, MinAliasIsHonored) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "min", "7");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 50, 80, 90, dummy_object{});

  ASSERT_EQ(list.size(), 1u);
  ASSERT_TRUE(list.front().float_value->minimum.is_initialized());
  EXPECT_DOUBLE_EQ(*list.front().float_value->minimum, 7.0);
}

TEST(SimplePerfGenerator, MaxAliasIsHonored) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "max", "999");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 50, 80, 90, dummy_object{});

  ASSERT_EQ(list.size(), 1u);
  ASSERT_TRUE(list.front().float_value->maximum.is_initialized());
  EXPECT_DOUBLE_EQ(*list.front().float_value->maximum, 999.0);
}

TEST(SimplePerfGenerator, MinimumWinsOverMinAlias) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  // Both names set: the spelled-out canonical name takes precedence so users
  // who set both don't get surprised by alphabetic ordering.
  native(factory)->set_option("counter", "minimum", "0");
  native(factory)->set_option("counter", "min", "999");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 1, 2, 3, dummy_object{});
  EXPECT_DOUBLE_EQ(*list.front().float_value->minimum, 0.0);
}

TEST(SimplePerfGenerator, MaximumWinsOverMaxAlias) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "maximum", "100");
  native(factory)->set_option("counter", "max", "999");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 1, 2, 3, dummy_object{});
  EXPECT_DOUBLE_EQ(*list.front().float_value->maximum, 100.0);
}

TEST(SimplePerfGenerator, BoundsViaWildcardSelector) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  // Wildcard selector: applies to every counter unless a more-specific
  // selector wins.
  native(factory)->set_option("*", "minimum", "0");
  native(factory)->set_option("*", "maximum", "100");
  gen.configure("anything", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "anything", 5, 50, 90, dummy_object{});
  EXPECT_DOUBLE_EQ(*list.front().float_value->minimum, 0.0);
  EXPECT_DOUBLE_EQ(*list.front().float_value->maximum, 100.0);
}

TEST(SimplePerfGenerator, GarbageBoundIsIgnored) {
  // Non-numeric input must not crash and must not produce a phantom bound.
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "minimum", "not-a-number");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 50, 80, 90, dummy_object{});
  EXPECT_FALSE(list.front().float_value->minimum.is_initialized());
}

TEST(SimplePerfGenerator, NegativeMinimumIsAllowed) {
  // Some metrics are signed (e.g. relative values, deltas). Don't reject.
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "minimum", "-50");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 0, 10, 20, dummy_object{});
  EXPECT_DOUBLE_EQ(*list.front().float_value->minimum, -50.0);
}

TEST(SimplePerfGenerator, FloatBoundIsPreserved) {
  int_perf_gen gen("", "", "");
  auto factory = make_factory();
  native(factory)->set_option("counter", "maximum", "12.5");
  gen.configure("counter", factory);

  perf_list_type list;
  gen.eval(list, nullptr, "counter", 1, 2, 3, dummy_object{});
  EXPECT_DOUBLE_EQ(*list.front().float_value->maximum, 12.5);
}

}  // namespace
