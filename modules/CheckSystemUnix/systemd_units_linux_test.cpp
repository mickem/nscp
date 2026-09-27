// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "systemd_units_linux.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

TEST(systemd_units, lists_broken_units_but_skips_missing_references) {
  const auto names = systemd_units::list_services([](const auto &) {
    return "healthy.service loaded active running Healthy service\n"
           "stale.service not-found inactive dead Missing dependency\n"
           "broken.service bad-setting inactive dead Broken service\n";
  });
  EXPECT_EQ(names, (std::vector<std::string>{"healthy.service", "broken.service"}));
}

TEST(systemd_units, malformed_listing_rows_do_not_become_empty_inventories) {
  for (const std::string row : {"demo.service", "demo.service loaded", "garbage demo.service loaded inactive dead", "demo.socket loaded inactive dead"}) {
    EXPECT_THROW(systemd_units::list_services([&](const auto &) { return row; }), std::runtime_error) << row;
  }
}

TEST(systemd_units, full_metadata_queries_for_checks_are_batched) {
  std::vector<std::string> names;
  for (int i = 0; i < 260; ++i) names.push_back("unit" + std::to_string(i) + ".service");
  int calls = 0;
  const auto rows = systemd_units::show_services(names, [&](const auto &argv) {
    ++calls;
    EXPECT_EQ(argv[0], "show");
    EXPECT_EQ(argv[2], "--");
    EXPECT_LE(argv.size(), 131u);
    std::string output;
    for (std::size_t i = 3; i < argv.size(); ++i) output += "Id=" + argv[i] + "\nLoadState=loaded\nActiveState=active\nSubState=running\n\n";
    return output;
  });
  EXPECT_EQ(calls, 3);
  ASSERT_EQ(rows.size(), 260u);
  EXPECT_EQ(rows.back().name, "unit259");
  EXPECT_TRUE(rows.back().is_started());
}
