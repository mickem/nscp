// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <atomic>
#include <future>
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
  std::atomic<int> registered{0};
  void register_command(const std::string, const std::string &, const std::string &) override { ++registered; }
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

TEST(script_manager, unload_all_parks_scripts_still_running_and_frees_them_later) {
  // unload_all() waits 5 s for other threads to leave their scripts. One
  // still inside after that used to have the script deleted under it. The
  // scripts are now parked - out of the manager, not unloaded, not freed -
  // and the next unload_all() that finds nothing running frees them, so
  // nothing is leaked for good (scripts_test runs under LeakSanitizer).
  auto runtime = std::make_shared<counting_runtime>();
  auto nscp = std::make_shared<fake_nscp_runtime>();
  scripts::script_manager<fake_traits> manager(runtime, nscp, 1, "test");
  scripts::script_information<fake_traits> *info = manager.add("alias", "script");

  std::atomic<bool> entered_ok{false};
  std::promise<void> entered, release;
  std::thread runner([&] {
    scripts::script_manager<fake_traits>::dispatch_guard guard(manager);
    entered_ok = guard.entered();
    // Signalled whatever happened, so a failure cannot leave the test
    // waiting forever for a thread that never got in.
    entered.set_value();
    if (entered_ok) release.get_future().wait();
  });
  entered.get_future().wait();
  if (!entered_ok) {
    runner.join();
    FAIL() << "the runner thread could not enter the manager";
  }

  manager.unload_all();
  EXPECT_EQ(0, runtime->unloaded.load()) << "a script another thread is still running was unloaded and freed";
  EXPECT_TRUE(manager.empty()) << "the scripts are out of the manager either way";

  // A load or start still walking the parked script may register commands
  // from it; they must not refill the map unload_all() cleared, nor reach
  // the core after the unload.
  info->register_command("type", "command", "description", fake_traits::function_type());
  EXPECT_FALSE(manager.find_command("type", "command")) << "a parked script registered a command";
  EXPECT_EQ(0, nscp->registered.load());

  release.set_value();
  runner.join();

  manager.unload_all();
  EXPECT_EQ(1, runtime->unloaded.load()) << "the parked script is unloaded and freed once nothing runs it";
}
