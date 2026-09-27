// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "task_facts.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_test_helper.hpp>
#include <stdexcept>

using namespace nscapi::facts::testing;

TEST(task_facts, full_paths_distinguish_equal_titles_and_include_disabled_hidden_tasks) {
  std::vector<task_facts::task> tasks = {{"\\B\\Backup", "Backup", "\\B", false, 1}, {"\\A\\Backup", "Backup", "\\A", true, 0}};
  nscapi::facts::response out;
  task_facts::publish(tasks, 1700000000, out);
  const auto message = out.to_message();
  const auto *set = find_set(message, "tasks");
  ASSERT_NE(set, nullptr);
  const auto &list = nscapi::facts::tree::get(set->facts(), "scheduled")->list_value();
  ASSERT_EQ(list.values_size(), 2);
  EXPECT_EQ(nscapi::facts::tree::get(list.values(0).object_value(), "id")->string_value(), "\\A\\Backup");
  EXPECT_FALSE(nscapi::facts::tree::get(list.values(1).object_value(), "enabled")->bool_value());
  EXPECT_TRUE(nscapi::facts::tree::get(list.values(1).object_value(), "hidden")->bool_value());
  // Only inventory fields: adding runtime or action data must be deliberate.
  EXPECT_EQ(list.values(1).object_value().fields_size(), 5);
  std::reverse(tasks.begin(), tasks.end());
  nscapi::facts::response again;
  task_facts::publish(tasks, 1700000001, again);
  EXPECT_EQ(json_of(out, "tasks"), json_of(again, "tasks"));
  EXPECT_EQ(gathered_of(out, "tasks"), "2023-11-14T22:13:20Z");
}

TEST(task_facts, unknown_legacy_hidden_flag_is_omitted) {
  nscapi::facts::response out;
  task_facts::publish({{"\\Legacy.job", "Legacy.job", "\\", false, -1}}, 0, out);
  EXPECT_EQ(json_of(out, "tasks").find("hidden"), std::string::npos);
  EXPECT_NE(json_of(out, "tasks").find("\"enabled\":false"), std::string::npos);
}

TEST(task_facts, invalid_identity_does_not_publish_a_partial_snapshot) {
  nscapi::facts::response out;
  EXPECT_THROW(task_facts::publish({{"\\A", "A", "\\", true, 0}, {"\\A", "A", "\\", false, 1}}, 0, out), std::runtime_error);
  EXPECT_EQ(json_of(out, "tasks"), "(no such set)");
  EXPECT_THROW(task_facts::publish({{"", "A", "\\", true, 0}}, 0, out), std::runtime_error);
}

TEST(task_facts, empty_inventory_and_truncation) {
  nscapi::facts::response empty;
  task_facts::publish({}, 0, empty);
  EXPECT_EQ(json_of(empty, "tasks"), "{\"scheduled\":[]}");
  std::vector<task_facts::task> tasks;
  for (std::size_t i = 0; i <= task_facts::max_tasks; ++i) tasks.push_back({"\\Task" + std::to_string(i), "Task", "\\", false, 0});
  nscapi::facts::response out;
  task_facts::publish(tasks, 1700000000, out);
  const auto message = out.to_message();
  const auto *set = find_set(message, "tasks");
  ASSERT_NE(set, nullptr);
  EXPECT_EQ(nscapi::facts::tree::get(set->facts(), "scheduled")->list_value().values_size(), task_facts::max_tasks);
  EXPECT_NE(error_of(out, "tasks").find("2501"), std::string::npos);
}
