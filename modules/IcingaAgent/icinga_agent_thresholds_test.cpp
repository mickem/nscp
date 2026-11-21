// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_thresholds.hpp"

#include <gtest/gtest.h>
#include <parsers/filter/threshold_shorthand.hpp>

using namespace icinga_agent;

namespace {
// What the range means to a filter check whose short form bounds `v`: the
// expression the -w / -c parser makes of it.
std::string expanded(const std::string &range, const modern_filter::shorthand::direction dir = modern_filter::shorthand::above) {
  const modern_filter::shorthand::spec_list specs{modern_filter::shorthand::spec("v", dir)};
  std::vector<std::string> expressions;
  std::string error;
  if (!modern_filter::shorthand::expand(specs, {}, modern_filter::shorthand::warning, range, expressions, error)) return "error: " + error;
  return expressions.size() == 1 ? expressions.front() : "?";
}
std::string range(const std::string &threshold, const bool inverted, const std::string &suffix = "", const bool keep_percent = true) {
  return to_range(parse_plugin_threshold(threshold), inverted, suffix, keep_percent);
}
}  // namespace

TEST(IcingaAgentThresholds, Parses) {
  plugin_threshold t = parse_plugin_threshold(" 20 ");
  EXPECT_FALSE(t.negated);
  EXPECT_FALSE(t.is_range);
  EXPECT_FALSE(t.percent);
  EXPECT_EQ(t.upper, "20");

  t = parse_plugin_threshold("!12.5%");
  EXPECT_TRUE(t.negated);
  EXPECT_TRUE(t.percent);
  EXPECT_EQ(t.upper, "12.5");

  t = parse_plugin_threshold("![10%-20%]");
  EXPECT_TRUE(t.negated);
  EXPECT_TRUE(t.is_range);
  EXPECT_TRUE(t.percent);
  EXPECT_EQ(t.lower, "10");
  EXPECT_EQ(t.upper, "20");

  // Equal ends are a single value to the plugin.
  EXPECT_FALSE(parse_plugin_threshold("[5-5]").is_range);
}

TEST(IcingaAgentThresholds, RefusesWhatThePluginRefuses) {
  for (const char *bad : {"", "!", "abc", "10MB", "[10]", "[10-20-30]", "[10%-20]", "[a-b]", "20%%", "1e3"}) {
    EXPECT_THROW(parse_plugin_threshold(bad), threshold_error) << bad;
  }
}

// threshold::rend() in Icinga's plugins/thresholds.cpp, for a plugin that
// alerts on the high side: load, procs, users, uptime, network, perfmon, ping.
TEST(IcingaAgentThresholds, PlainReading) {
  EXPECT_EQ(range("20", false), "~:20");
  EXPECT_EQ(range("!20", false), "@~:20");
  EXPECT_EQ(range("[10-20]", false), "@10:20");
  EXPECT_EQ(range("![10-20]", false), "10:20");

  // ...and what the filter check makes of them, whichever way its own short
  // form leans: every range says its side itself.
  for (const auto dir : {modern_filter::shorthand::above, modern_filter::shorthand::below}) {
    EXPECT_EQ(expanded(range("20", false), dir), "v > 20");
    EXPECT_EQ(expanded(range("!20", false), dir), "v <= 20");
    EXPECT_EQ(expanded(range("[10-20]", false), dir), "v >= 10 and v <= 20");
    EXPECT_EQ(expanded(range("![10-20]", false), dir), "v < 10 or v > 20");
  }
}

// check_disk.exe, check_memory.exe and check_swap.exe flip the reading: the
// threshold is on free space.
TEST(IcingaAgentThresholds, InvertedReading) {
  EXPECT_EQ(expanded(range("20%", true)), "v <= 20%");
  EXPECT_EQ(expanded(range("!20%", true)), "v > 20%");
  EXPECT_EQ(expanded(range("[10-20]", true, "m")), "v < 10m or v > 20m");
  EXPECT_EQ(expanded(range("![10-20]", true, "m")), "v >= 10m and v <= 20m");
}

TEST(IcingaAgentThresholds, Units) {
  // A bare number is in the plugin's unit; a percentage is not.
  EXPECT_EQ(range("500", true, "m"), "@~:500m");
  EXPECT_EQ(range("20%", true, "m"), "@~:20%");
  // For a keyword that already is a percentage the sign is dropped.
  EXPECT_EQ(range("80%", false, "", false), "~:80");
  EXPECT_EQ(range("-5", false), "~:-5");
}

TEST(IcingaAgentThresholds, Expressions) {
  const auto expression = [](const std::string &threshold, const bool inverted) {
    return to_expression(parse_plugin_threshold(threshold), inverted, "used", "g", true);
  };
  EXPECT_EQ(expression("80%", false), "used > 80%");
  EXPECT_EQ(expression("!80", false), "used <= 80g");
  EXPECT_EQ(expression("[10-20]", false), "used >= 10g and used <= 20g");
  EXPECT_EQ(expression("![10-20]", false), "used < 10g or used > 20g");
  EXPECT_EQ(expression("20%", true), "used <= 20%");
}
