// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "disable_list.hpp"

#include <gtest/gtest.h>

// Regression for #1368: "cpu" is a substring of "cpu_frequency", so the old
// substring matching disabled the CPU load sampling whenever only the CPU
// frequency collector was asked to be disabled.
TEST(disable_list, cpu_frequency_does_not_disable_cpu) {
  const auto tokens = disable_list::parse("cpu_frequency");
  EXPECT_EQ(tokens.count("cpu_frequency"), 1u);
  EXPECT_EQ(tokens.count("cpu"), 0u);
}

TEST(disable_list, cpu_does_not_disable_cpu_frequency) {
  const auto tokens = disable_list::parse("cpu");
  EXPECT_EQ(tokens.count("cpu"), 1u);
  EXPECT_EQ(tokens.count("cpu_frequency"), 0u);
}

TEST(disable_list, splits_on_comma_and_trims) {
  const auto tokens = disable_list::parse(" temperature , cpu_frequency,\tbattery ");
  EXPECT_EQ(tokens.size(), 3u);
  EXPECT_EQ(tokens.count("temperature"), 1u);
  EXPECT_EQ(tokens.count("cpu_frequency"), 1u);
  EXPECT_EQ(tokens.count("battery"), 1u);
}

TEST(disable_list, empty_and_blank_entries_are_ignored) {
  EXPECT_TRUE(disable_list::parse("").empty());
  EXPECT_TRUE(disable_list::parse(" , ,").empty());
  const auto tokens = disable_list::parse("cpu,,network");
  EXPECT_EQ(tokens.size(), 2u);
}

TEST(disable_list, all_documented_values_are_known) {
  const auto tokens = disable_list::parse("battery,cpu,handles,network,temperature,cpu_frequency,os_updates,metrics,pdh");
  for (const std::string &token : tokens) {
    EXPECT_EQ(disable_list::known_tokens().count(token), 1u) << "unexpected token: " << token;
  }
  EXPECT_EQ(tokens.size(), disable_list::known_tokens().size());
}

TEST(disable_list, unknown_tokens_are_reported_by_lookup) {
  const auto tokens = disable_list::parse("cpufrequency");
  EXPECT_EQ(tokens.count("cpufrequency"), 1u);
  EXPECT_EQ(disable_list::known_tokens().count("cpufrequency"), 0u);
}
