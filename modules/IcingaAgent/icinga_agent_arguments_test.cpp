// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_arguments.hpp"

#include <gtest/gtest.h>

#include "icinga_agent_macros.hpp"

namespace json = boost::json;
using namespace icinga_agent;

namespace {
using tokens = std::list<std::string>;
}

TEST(IcingaAgentArguments, StripDashes) {
  EXPECT_EQ(strip_dashes("--warning"), "warning");
  EXPECT_EQ(strip_dashes("-w"), "w");
  EXPECT_EQ(strip_dashes("warning"), "warning");
  EXPECT_EQ(strip_dashes("---"), "");
  EXPECT_EQ(strip_dashes("a-b"), "a-b");
}

TEST(IcingaAgentArguments, ParsesAPlainValue) {
  const argument_spec spec = parse_argument_spec("--warning", json::value("$w$"));
  EXPECT_EQ(spec.key, "warning");
  ASSERT_TRUE(spec.value.has_value());
  EXPECT_EQ(spec.value.value(), json::value("$w$"));
  EXPECT_FALSE(spec.set_if.has_value());
}

TEST(IcingaAgentArguments, ParsesEveryAttribute) {
  const argument_spec spec =
      parse_argument_spec("-x", json::parse(R"({"value":"$v$","set_if":"$s$","order":"3","skip_key":1,"repeat_key":false,"key":"--real","required":true})"));
  EXPECT_EQ(spec.key, "real");
  EXPECT_EQ(spec.order, 3);
  EXPECT_TRUE(spec.skip_key);
  EXPECT_FALSE(spec.repeat_key);
  EXPECT_TRUE(spec.required);
  EXPECT_TRUE(spec.set_if.has_value());
  EXPECT_TRUE(spec.value.has_value());
}

TEST(IcingaAgentArguments, ANullValueIsAFlag) {
  EXPECT_FALSE(parse_argument_spec("--all", json::parse(R"({"value":null})")).value.has_value());
  EXPECT_FALSE(parse_argument_spec("--all", json::value(nullptr)).value.has_value());
}

TEST(IcingaAgentArguments, SetIf) {
  const json::object macros{{"on", true}, {"off", false}, {"zero", "0"}};
  argument_spec spec;
  EXPECT_TRUE(argument_enabled(spec, macros));  // no set_if
  spec.set_if = json::value("$on$");
  EXPECT_TRUE(argument_enabled(spec, macros));
  spec.set_if = json::value("$off$");
  EXPECT_FALSE(argument_enabled(spec, macros));
  spec.set_if = json::value("$zero$");
  EXPECT_FALSE(argument_enabled(spec, macros));
  spec.set_if = json::value("$missing$");
  EXPECT_FALSE(argument_enabled(spec, macros));
  spec.set_if = json::value(true);
  EXPECT_TRUE(argument_enabled(spec, macros));
}

TEST(IcingaAgentArguments, ResolvesValues) {
  const json::object macros{{"w", "load > 80"}, {"list", json::array{"a", "b"}}, {"empty", ""}, {"n", 5}};
  argument_spec spec;
  spec.value = json::value("$w$");
  EXPECT_EQ(resolve_argument_values(spec, macros).value(), std::vector<std::string>{"load > 80"});
  spec.value = json::value("$list$");
  EXPECT_EQ(resolve_argument_values(spec, macros).value(), (std::vector<std::string>{"a", "b"}));
  spec.value = json::value("$n$");
  EXPECT_EQ(resolve_argument_values(spec, macros).value(), std::vector<std::string>{"5"});
  spec.value = json::value("$empty$");
  EXPECT_FALSE(resolve_argument_values(spec, macros).has_value());
  spec.value = json::value("$missing$");
  EXPECT_FALSE(resolve_argument_values(spec, macros).has_value());
  spec.value = boost::none;  // a flag
  EXPECT_TRUE(resolve_argument_values(spec, macros).value().empty());
}

TEST(IcingaAgentArguments, ARequiredArgumentWithAMissingMacroIsAnError) {
  argument_spec spec;
  spec.key = "host";
  spec.required = true;
  spec.value = json::value("$missing$");
  EXPECT_THROW(resolve_argument_values(spec, json::object()), argument_error);
}

TEST(IcingaAgentArguments, Rendering) {
  argument_spec spec;
  spec.key = "time";
  EXPECT_EQ(render_argument(spec, {"5m"}), tokens{"time=5m"});
  EXPECT_EQ(render_argument(spec, {"5m", "1h"}), (tokens{"time=5m", "time=1h"}));
  spec.repeat_key = false;
  EXPECT_EQ(render_argument(spec, {"5m", "1h"}), tokens{"time=5m,1h"});
  spec.skip_key = true;
  EXPECT_EQ(render_argument(spec, {"5m", "1h"}), (tokens{"5m", "1h"}));
  argument_spec flag;
  flag.key = "show-all";
  EXPECT_EQ(render_argument(flag, {}), tokens{"show-all"});
  EXPECT_TRUE(render_argument(argument_spec(), {}).empty());  // no key, no value
}

TEST(IcingaAgentArguments, BuildsAWholeBlockLikeIcinga) {
  const json::object macros{{"warn", "load > 80"}, {"crit", ""}, {"show", true}, {"hide", false}, {"filters", json::array{"a", "b"}}};
  const json::object args{
      {"warning", json::object{{"value", "$warn$"}, {"order", 1}}},
      {"critical", json::object{{"value", "$crit$"}, {"order", 2}}},     // empty -> dropped
      {"--show-all", json::object{{"set_if", "$show$"}, {"order", 3}}},  // flag, dashes stripped
      {"hidden", json::object{{"set_if", "$hide$"}}},                    // set_if false -> dropped
      {"missing", json::object{{"value", "$nope$"}}},                    // macro not sent -> dropped
      {"filter", json::object{{"value", "$filters$"}, {"order", 4}}},    // repeated key
      {"positional", json::object{{"value", "x"}, {"skip_key", true}, {"order", 5}}},
  };
  EXPECT_EQ(build_arguments(args, macros), (tokens{"warning=load > 80", "show-all", "filter=a", "filter=b", "x"}));
}

TEST(IcingaAgentArguments, EqualOrderSortsByKey) {
  const json::object args{{"b", "2"}, {"a", "1"}, {"c", json::object{{"value", "3"}, {"order", -1}}}};
  EXPECT_EQ(build_arguments(args, json::object()), (tokens{"c=3", "a=1", "b=2"}));
}

TEST(IcingaAgentArguments, AnUnterminatedMacroIsAnError) {
  const json::object args{{"x", "$oops"}};
  EXPECT_THROW(build_arguments(args, json::object()), macro_syntax_error);
}
