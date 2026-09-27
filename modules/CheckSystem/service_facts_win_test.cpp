// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/service_facts.hpp>
#include <set>
#include <win/services.hpp>

TEST(service_facts_win, inventories_all_services_by_the_check_identity) {
  const auto services = service_facts::gather();
  ASSERT_FALSE(services.empty());
  std::set<std::string> ids;
  for (const auto &service : services) {
    EXPECT_FALSE(service.name.empty());
    EXPECT_TRUE(ids.insert(service.name).second);
    EXPECT_FALSE(service.start_type.empty());
  }
  // EventLog is a core Windows service, whether started or stopped.
  EXPECT_EQ(ids.count("EventLog"), 1u);
}
