// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <scripts/script_interface.hpp>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

// script_manager is a template over the script language; a trait with no
// interpreter behind it is enough to drive the bookkeeping.
struct fake_traits {
  struct user_data_type {};
  struct function_type {};
};

struct counting_runtime : public scripts::script_runtime_interface<fake_traits> {
  std::atomic<int> unloaded{0};
  void load(scripts::script_information<fake_traits> *) override {}
  void start(scripts::script_information<fake_traits> *) override {}
  void unload(scripts::script_information<fake_traits> *) override { ++unloaded; }
  void create_user_data(scripts::script_information<fake_traits> *) override {}
};

struct fake_nscp_runtime : public scripts::nscp_runtime_interface {
  void register_command(const std::string, const std::string &, const std::string &) override {}
  std::shared_ptr<scripts::settings_provider> get_settings_provider() override { return {}; }
  std::shared_ptr<scripts::core_provider> get_core_provider() override { return {}; }
};

}  // namespace

TEST(script_manager, concurrent_adds_get_distinct_ids_and_are_all_unloaded) {
  // Two `nscp lua execute` callers add scripts at the same time. A shared id
  // makes the second insert replace the first in the script map, and that
  // script is then never unloaded - so every script added must come back out
  // of unload_all(), each under an id of its own.
  auto runtime = std::make_shared<counting_runtime>();
  scripts::script_manager<fake_traits> manager(runtime, std::make_shared<fake_nscp_runtime>(), 1, "test");

  const int threads = 8;
  const int per_thread = 200;
  std::mutex ids_mutex;
  std::set<int> ids;
  std::vector<std::thread> workers;
  for (int t = 0; t < threads; ++t) {
    workers.emplace_back([&] {
      for (int i = 0; i < per_thread; ++i) {
        const int id = manager.add("alias", "script")->script_id;
        std::lock_guard<std::mutex> lock(ids_mutex);
        ids.insert(id);
      }
    });
  }
  for (auto &w : workers) w.join();

  EXPECT_EQ(static_cast<std::size_t>(threads * per_thread), ids.size());
  manager.unload_all();
  EXPECT_EQ(threads * per_thread, runtime->unloaded.load());
}
