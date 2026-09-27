// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_test_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <set>

#include "task_facts.hpp"

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

TEST(task_facts_win, live_collection_uses_full_paths_and_inventory_fields) {
  const auto tasks = task_facts::gather();
  std::set<std::string> ids;
  for (const auto &task : tasks) {
    ASSERT_FALSE(task.id.empty());
    EXPECT_EQ(task.id.front(), '\\');
    EXPECT_TRUE(ids.insert(task.id).second);
    EXPECT_FALSE(task.name.empty());
    EXPECT_FALSE(task.folder.empty());
    EXPECT_GE(task.hidden, -1);
    EXPECT_LE(task.hidden, 1);
  }
  nscapi::facts::response out;
  EXPECT_NO_THROW(task_facts::publish(tasks, 1700000000, out));
  EXPECT_NE(nscapi::facts::testing::json_of(out, "tasks"), "(no facts)");
}
