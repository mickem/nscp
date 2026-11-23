// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_json.hpp"

#include <gtest/gtest.h>

namespace json = boost::json;
using namespace icinga_agent::json_util;

TEST(IcingaAgentJson, ToTextRendersScalarsLikeACommandLine) {
  EXPECT_EQ(to_text(json::value("x")), "x");
  EXPECT_EQ(to_text(json::value(true)), "true");
  EXPECT_EQ(to_text(json::value(false)), "false");
  EXPECT_EQ(to_text(json::value(-3)), "-3");
  EXPECT_EQ(to_text(json::value(std::uint64_t{18446744073709551615ull})), "18446744073709551615");
  EXPECT_EQ(to_text(json::value(nullptr)), "");
}

TEST(IcingaAgentJson, IntegralDoublesLoseTheirFraction) {
  // Icinga's JSON writer sends a threshold of 80 as 80.0.
  EXPECT_EQ(to_text(json::value(80.0)), "80");
  EXPECT_EQ(to_text(json::value(-1.0)), "-1");
  EXPECT_EQ(to_text(json::value(0.5)), "0.5");
  EXPECT_EQ(to_text(json::value(1.0 / 3.0)), "0.333333333333333");
}

TEST(IcingaAgentJson, ContainersAreSerialised) {
  EXPECT_EQ(to_text(json::parse(R"([1,"a"])")), R"([1,"a"])");
  EXPECT_EQ(to_text(json::parse(R"({"k":1})")), R"({"k":1})");
}

TEST(IcingaAgentJson, FindAndTextField) {
  const json::object obj{{"a", 1}, {"b", "x"}};
  ASSERT_NE(find(obj, "a"), nullptr);
  EXPECT_EQ(find(obj, "missing"), nullptr);
  EXPECT_EQ(text_field(obj, "a"), "1");
  EXPECT_EQ(text_field(obj, "missing"), "");
}

TEST(IcingaAgentJson, EmptinessFollowsIcinga) {
  EXPECT_TRUE(is_empty(json::value(nullptr)));
  EXPECT_TRUE(is_empty(json::value("")));
  EXPECT_TRUE(is_empty(json::array()));
  EXPECT_TRUE(is_empty(json::object()));
  EXPECT_FALSE(is_empty(json::value(0)));
  EXPECT_FALSE(is_empty(json::value(false)));
  EXPECT_FALSE(is_empty(json::value(" ")));
}

TEST(IcingaAgentJson, TruthinessFollowsIcinga) {
  for (const auto &v : {json::value(true), json::value(1), json::value(2.5), json::value("1"), json::value("yes"), json::value(json::array{1})}) {
    EXPECT_TRUE(is_truthy(v)) << json::serialize(v);
  }
  for (const auto &v : {json::value(false), json::value(0), json::value(0.0), json::value(""), json::value("0"), json::value("false"), json::value(nullptr),
                        json::value(json::array())}) {
    EXPECT_FALSE(is_truthy(v)) << json::serialize(v);
  }
}

TEST(IcingaAgentJson, ToLongAcceptsNumbersAndNumericStrings) {
  EXPECT_EQ(to_long(json::value(3), 0), 3);
  EXPECT_EQ(to_long(json::value(-2.0), 0), -2);
  EXPECT_EQ(to_long(json::value("7"), 0), 7);
  EXPECT_EQ(to_long(json::value("-7"), 0), -7);
  EXPECT_EQ(to_long(json::value("7x"), 9), 9);
  EXPECT_EQ(to_long(json::value(""), 9), 9);
  EXPECT_EQ(to_long(json::value(true), 9), 9);
}

// What a translation reads of the macros a master sends: missing and empty
// are the same thing.
TEST(IcingaAgentJson, ReadersTreatMissingAndEmptyAlike) {
  const json::object macros = json::parse(R"({"warn":"20%","list":["C:","D:"],"count":5,"on":true,"off":"0","blank":"","none":null,"nothing":[]})").as_object();

  for (const char *key : {"warn", "list", "count", "on", "off"}) EXPECT_TRUE(has_value(macros, key)) << key;
  for (const char *key : {"blank", "none", "nothing", "absent"}) EXPECT_FALSE(has_value(macros, key)) << key;

  for (const char *key : {"warn", "list", "count", "on"}) EXPECT_TRUE(truthy_field(macros, key)) << key;
  for (const char *key : {"off", "blank", "none", "nothing", "absent"}) EXPECT_FALSE(truthy_field(macros, key)) << key;

  EXPECT_EQ(text_list(macros, "list"), (std::vector<std::string>{"C:", "D:"}));
  EXPECT_EQ(text_list(macros, "warn"), (std::vector<std::string>{"20%"}));
  EXPECT_EQ(text_list(macros, "count"), (std::vector<std::string>{"5"}));
  for (const char *key : {"blank", "none", "nothing", "absent"}) EXPECT_TRUE(text_list(macros, key).empty()) << key;
}
