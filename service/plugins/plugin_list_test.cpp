// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "plugin_list.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "plugin_interface.hpp"

// Mock logger for testing
class MockPluginListLogger : public nsclient::logging::log_interface {
 public:
  void trace(const std::string&, const char*, int, const std::string&) override {}
  void debug(const std::string&, const char*, int, const std::string&) override {}
  void info(const std::string&, const char*, int, const std::string&) override {}
  void warning(const std::string&, const char*, int, const std::string&) override {}
  void error(const std::string&, const char*, int, const std::string&) override {}
  void critical(const std::string&, const char*, int, const std::string&) override {}
  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return false; }
  bool should_critical() const override { return false; }
};

// Mock plugin for testing
class MockListPlugin : public nsclient::core::plugin_interface {
  std::string module_;
  std::string name_;

 public:
  MockListPlugin(unsigned int id, const std::string& alias, const std::string& module) : plugin_interface(id, alias), module_(module), name_(module) {}

  bool load_plugin(NSCAPI::moduleLoadMode) override { return true; }
  bool has_start() override { return false; }
  bool start_plugin() override { return true; }
  bool has_prepare_shutdown() override { return false; }
  void prepare_shutdown_plugin() override {}
  void unload_plugin() override {}
  std::string getName() override { return name_; }
  std::string getDescription() override { return "Mock plugin for testing"; }
  std::string get_version() override { return "1.0.0"; }
  bool hasCommandHandler() override { return false; }
  NSCAPI::nagiosReturn handleCommand(std::string, std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasNotificationHandler() override { return false; }
  NSCAPI::nagiosReturn handleNotification(const char*, std::string&, std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  NSCAPI::nagiosReturn handle_schedule(const std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMessageHandler() override { return false; }
  void handleMessage(const char*, unsigned int) override {}
  bool has_on_event() override { return false; }
  NSCAPI::nagiosReturn on_event(const std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMetricsFetcher() override { return false; }
  bool hasFactsFetcher() override { return false; }
  NSCAPI::nagiosReturn fetchFacts(const std::string&, std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  NSCAPI::nagiosReturn fetchMetrics(std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool hasMetricsSubmitter() override { return false; }
  NSCAPI::nagiosReturn submitMetrics(const std::string&) override { return NSCAPI::cmd_return_codes::returnIgnored; }
  bool has_command_line_exec() override { return false; }
  int commandLineExec(bool, std::string&, std::string&) override { return 0; }
  bool has_routing_handler() override { return false; }
  bool route_message(const char*, const char*, unsigned int, char**, char**, unsigned int*) override { return false; }
  bool is_duplicate(boost::filesystem::path file, std::string alias) override { return module_ == file.string() && get_alias() == alias; }
  std::string getModule() override { return module_; }
  void on_log_message(const std::string&) override {}
};

// ============================================================================
// plugins_list_exception tests
// ============================================================================

TEST(PluginsListExceptionTest, ConstructWithMessage) {
  const nsclient::plugins_list_exception ex("Test error message");
  EXPECT_STREQ(ex.what(), "Test error message");
}

TEST(PluginsListExceptionTest, ConstructWithEmptyMessage) {
  const nsclient::plugins_list_exception ex("");
  EXPECT_STREQ(ex.what(), "");
}

TEST(PluginsListExceptionTest, InheritsFromStdException) {
  const nsclient::plugins_list_exception ex("Error");
  const std::exception* base_ptr = &ex;
  EXPECT_NE(base_ptr->what(), nullptr);
}

TEST(PluginsListExceptionTest, ThrowAndCatch) {
  try {
    throw nsclient::plugins_list_exception("Plugin list error");
  } catch (const nsclient::plugins_list_exception& ex) {
    EXPECT_STREQ(ex.what(), "Plugin list error");
  }
}

TEST(PluginsListExceptionTest, ThrowAndCatchAsStdException) {
  try {
    throw nsclient::plugins_list_exception("Base exception test");
  } catch (const std::exception& ex) {
    EXPECT_STREQ(ex.what(), "Base exception test");
  }
}

// ============================================================================
// simple_plugins_list tests
// ============================================================================

// What the plugin manager does to take a module out of one walk list: close
// its slot, wait for the rounds inside it, and finish on that outcome.
enum class removal { absent, removed, still_walking };
removal remove_from(nsclient::simple_plugins_list& list, const unsigned long id, const std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
  const nsclient::simple_plugins_list::closing c = list.close_plugin(id);
  if (!c) return removal::absent;
  const bool drained = nsclient::simple_plugins_list::drain_each({c}, timeout)[0];
  c.finish(drained);
  return drained ? removal::removed : removal::still_walking;
}

class SimplePluginsListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = std::make_shared<MockPluginListLogger>();
    list_ = std::make_unique<nsclient::simple_plugins_list>(logger_);
  }
  nsclient::logging::log_client_accessor logger_;
  std::unique_ptr<nsclient::simple_plugins_list> list_;
};

TEST_F(SimplePluginsListTest, InitialStateEmpty) { EXPECT_EQ(list_->to_string(), ""); }

TEST_F(SimplePluginsListTest, AddPlugin) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "test_alias", "TestModule");
  list_->add_plugin(plugin);
  EXPECT_EQ(list_->to_string(), "TestModule");
}

TEST_F(SimplePluginsListTest, AddMultiplePlugins) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("Module1") != std::string::npos);
  EXPECT_TRUE(result.find("Module2") != std::string::npos);
}

TEST_F(SimplePluginsListTest, AddDuplicateIdIgnored) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(1, "alias2", "Module2");  // Same ID
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  // Should only have one plugin
  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("Module1") != std::string::npos);
}

TEST_F(SimplePluginsListTest, RemoveAll) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  list_->remove_all();
  EXPECT_EQ(list_->to_string(), "");
}

TEST_F(SimplePluginsListTest, RemovePlugin) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  remove_from(*list_, 1);

  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("Module1") == std::string::npos);
  EXPECT_TRUE(result.find("Module2") != std::string::npos);
}

TEST_F(SimplePluginsListTest, RemoveNonExistentPlugin) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  // Removing non-existent plugin should not crash
  remove_from(*list_, 999);

  EXPECT_EQ(list_->to_string(), "Module");
}

TEST_F(SimplePluginsListTest, DoAllCallback) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  int count = 0;
  list_->do_all([&count](nsclient::plugin_type) { count++; });

  EXPECT_EQ(count, 2);
}

TEST_F(SimplePluginsListTest, DoAllOnEmptyList) {
  int count = 0;
  list_->do_all([&count](nsclient::plugin_type) { count++; });

  EXPECT_EQ(count, 0);
}

// The callback runs module code, and a module may load or unload another
// module from there (a metrics fetcher calling load_module). That re-enters
// add_plugin / remove_plugin on the thread do_all is running on, so do_all
// must not hold the list's lock across the call: with it held shared, the
// unique lock those want waited out its timeout and the change was dropped.
TEST_F(SimplePluginsListTest, DoAllCallbackMayRemoveAndAddPlugins) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  const auto plugin3 = std::make_shared<MockListPlugin>(3, "alias3", "Module3");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  std::vector<unsigned int> seen;
  list_->do_all([&](nsclient::plugin_type p) {
    seen.push_back(p->get_id());
    remove_from(*list_, p->get_id());
    if (p->get_id() == 1) list_->add_plugin(plugin3);
  });

  // Every plugin that was registered when the walk started was visited
  // exactly once; the one added during the walk is not visited this time.
  EXPECT_EQ(seen, (std::vector<unsigned int>{1, 2}));
  int count = 0;
  std::vector<unsigned int> remaining;
  list_->do_all([&](nsclient::plugin_type p) {
    count++;
    remaining.push_back(p->get_id());
  });
  EXPECT_EQ(count, 1);
  EXPECT_EQ(remaining, (std::vector<unsigned int>{3}));
}

// A removal on another thread still waits for a walk that may be calling the
// removed plugin, as the held lock used to make it: the plugin manager drops
// what the module contributed right after. A removal of an id that is not
// in the list has nothing to wait for and returns at once.
TEST_F(SimplePluginsListTest, RemovePluginWaitsForAWalkOnAnotherThread) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  list_->add_plugin(plugin1);

  std::mutex mu;
  std::condition_variable cv;
  bool entered = false;
  bool released = false;
  std::thread walk([&]() {
    list_->do_all([&](nsclient::plugin_type) {
      std::unique_lock<std::mutex> lock(mu);
      entered = true;
      cv.notify_all();
      cv.wait(lock, [&]() { return released; });
    });
  });
  {
    std::unique_lock<std::mutex> lock(mu);
    cv.wait(lock, [&]() { return entered; });
  }

  // Not in this list: nothing to close or wait for.
  EXPECT_EQ(remove_from(*list_, 42), removal::absent);

  std::atomic<bool> removed{false};
  std::thread remover([&]() {
    EXPECT_EQ(remove_from(*list_, 1), removal::removed);
    removed = true;
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_FALSE(removed.load());

  {
    std::lock_guard<std::mutex> lock(mu);
    released = true;
  }
  cv.notify_all();
  remover.join();
  walk.join();
  EXPECT_TRUE(removed.load());
  EXPECT_TRUE(list_->empty());
}

// A round is waited for only by the removal of the module it is inside: a
// round stuck in one module's fetchMetrics - or blocked on the lifecycle
// lock from inside it - does not hold up the removal of another, and the
// module removed meanwhile is skipped when the round reaches it.
TEST_F(SimplePluginsListTest, RemovingAModuleDoesNotWaitForARoundInsideAnother) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  std::mutex mu;
  std::condition_variable cv;
  bool entered = false;
  bool released = false;
  std::vector<unsigned int> called;
  std::thread walk([&]() {
    list_->do_all([&](nsclient::plugin_type p) {
      std::unique_lock<std::mutex> lock(mu);
      called.push_back(p->get_id());
      if (p->get_id() != 1) return;
      entered = true;
      cv.notify_all();
      cv.wait(lock, [&]() { return released; });
    });
  });
  {
    std::unique_lock<std::mutex> lock(mu);
    cv.wait(lock, [&]() { return entered; });
  }

  // The round is inside module 1. Removing module 2 has nothing to wait for:
  // with no wait allowed at all, it still comes back removed.
  EXPECT_EQ(remove_from(*list_, 2, std::chrono::milliseconds(0)), removal::removed);
  // Removing module 1 is what the round holds up.
  EXPECT_EQ(remove_from(*list_, 1, std::chrono::milliseconds(0)), removal::still_walking);

  {
    std::lock_guard<std::mutex> lock(mu);
    released = true;
  }
  cv.notify_all();
  walk.join();
  // Module 2 was removed before the round got to it, so it was never called.
  EXPECT_EQ(called, (std::vector<unsigned int>{1}));
}

// A refused removal reopens the module where it was, so the rounds keep
// walking the modules in the order they were registered.
TEST_F(SimplePluginsListTest, AReopenedModuleKeepsItsPlace) {
  for (unsigned int id = 1; id <= 3; ++id) list_->add_plugin(std::make_shared<MockListPlugin>(id, "alias", "Module"));

  const nsclient::simple_plugins_list::closing c = list_->close_plugin(2);
  ASSERT_TRUE(static_cast<bool>(c));
  std::vector<unsigned int> while_closed;
  list_->do_all([&](nsclient::plugin_type p) { while_closed.push_back(p->get_id()); });
  EXPECT_EQ(while_closed, (std::vector<unsigned int>{1, 3}));

  c.reopen();
  std::vector<unsigned int> reopened;
  list_->do_all([&](nsclient::plugin_type p) { reopened.push_back(p->get_id()); });
  EXPECT_EQ(reopened, (std::vector<unsigned int>{1, 2, 3}));
}

// The list, not a round, owns the module: once removed, the remover holds
// the last reference, so the module is never destroyed on a walking thread.
TEST_F(SimplePluginsListTest, TheRemoverHoldsTheLastReference) {
  auto plugin = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  list_->add_plugin(plugin);
  list_->do_all([](nsclient::plugin_type) {});
  EXPECT_EQ(remove_from(*list_, 1), removal::removed);
  EXPECT_EQ(plugin.use_count(), 1);
}

TEST_F(SimplePluginsListTest, RemoveAllNamesTheModulesARoundIsStillInside) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  std::mutex mu;
  std::condition_variable cv;
  bool entered = false;
  bool released = false;
  std::thread walk([&]() {
    list_->do_all([&](nsclient::plugin_type p) {
      if (p->get_id() != 1) return;
      std::unique_lock<std::mutex> lock(mu);
      entered = true;
      cv.notify_all();
      cv.wait(lock, [&]() { return released; });
    });
  });
  {
    std::unique_lock<std::mutex> lock(mu);
    cv.wait(lock, [&]() { return entered; });
  }
  const std::vector<nsclient::plugin_type> stuck = list_->remove_all(std::chrono::milliseconds(0));
  ASSERT_EQ(stuck.size(), 1u);
  EXPECT_EQ(stuck[0], plugin1);
  EXPECT_TRUE(list_->empty());
  {
    std::lock_guard<std::mutex> lock(mu);
    released = true;
  }
  cv.notify_all();
  walk.join();
}

// A module whose slot cannot be taken out keeps its module: finish() never
// empties a gate whose slot is still listed.
TEST_F(SimplePluginsListTest, FinishLeavesAStuckSlotHoldingItsModule) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  list_->add_plugin(plugin1);
  const nsclient::simple_plugins_list::closing c = list_->close_plugin(1);
  ASSERT_TRUE(static_cast<bool>(c));
  // Not drained: nothing handed back, the slot stays, closed, with its module.
  EXPECT_FALSE(c.finish(false));
  EXPECT_EQ(c.plugin(), plugin1);
  EXPECT_EQ(list_->to_string(), "Module1");
  // Drained: out of the list and handed back.
  EXPECT_EQ(c.finish(true), plugin1);
  EXPECT_TRUE(list_->empty());
}

// ============================================================================
// plugins_list_with_listener tests
// ============================================================================

class PluginsListWithListenerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logger_ = std::make_shared<MockPluginListLogger>();
    list_ = std::make_unique<nsclient::plugins_list_with_listener>(logger_);
  }
  nsclient::logging::log_client_accessor logger_;
  std::unique_ptr<nsclient::plugins_list_with_listener> list_;
};

TEST_F(PluginsListWithListenerTest, InitialStateEmpty) {
  const auto plugins = list_->list();
  EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginsListWithListenerTest, AddPlugin) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  // The plugin should be findable
  EXPECT_TRUE(list_->have_plugin(1));
}

TEST_F(PluginsListWithListenerTest, RegisterListenerForChannel) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  list_->register_listener(1, "test_channel");

  auto listeners = list_->get("test_channel");
  EXPECT_EQ(listeners.size(), 1u);
}

TEST_F(PluginsListWithListenerTest, RegisterListenerForMultipleChannels) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  list_->register_listener(1, "channel1,channel2");

  const auto listeners1 = list_->get("channel1");
  const auto listeners2 = list_->get("channel2");

  EXPECT_EQ(listeners1.size(), 1u);
  EXPECT_EQ(listeners2.size(), 1u);
}

TEST_F(PluginsListWithListenerTest, GetNonExistentChannelReturnsEmpty) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  const auto listeners = list_->get("nonexistent");
  EXPECT_TRUE(listeners.empty());
}

TEST_F(PluginsListWithListenerTest, RegisterListenerWithNonExistentPluginThrows) {
  // No plugins added
  EXPECT_THROW(list_->register_listener(999, "channel"), nsclient::plugins_list_exception);
}

TEST_F(PluginsListWithListenerTest, MultiplePluginsOnSameChannel) {
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);

  list_->register_listener(1, "shared_channel");
  list_->register_listener(2, "shared_channel");

  const auto listeners = list_->get("shared_channel");
  EXPECT_EQ(listeners.size(), 2u);
}

TEST_F(PluginsListWithListenerTest, RemovePluginRemovesFromListeners) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);
  list_->register_listener(1, "channel");

  list_->remove_plugin(1);

  // Sole subscriber gone, so the channel entry goes with it.
  const auto listeners = list_->get("channel");
  EXPECT_TRUE(listeners.empty());
}

TEST_F(PluginsListWithListenerTest, RemovePluginKeepsOtherSubscribersOnSameChannel) {
  // Unloading one module must not silently unsubscribe every other module from
  // the channels it happened to share.
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);
  list_->register_listener(1, "shared_channel");
  list_->register_listener(2, "shared_channel");

  list_->remove_plugin(1);

  const auto listeners = list_->get("shared_channel");
  ASSERT_EQ(listeners.size(), 1u);
  EXPECT_EQ(listeners.front()->get_id(), 2u);
}

TEST_F(PluginsListWithListenerTest, UnregisterListenerDropsOnlyThatSubscription) {
  // A module moving its subscription (SimpleFileWriter with a changed
  // `channel`) takes back the old one while staying loaded, and leaves every
  // other subscriber of that channel - and its own other channels - alone.
  const auto plugin1 = std::make_shared<MockListPlugin>(1, "alias1", "Module1");
  const auto plugin2 = std::make_shared<MockListPlugin>(2, "alias2", "Module2");
  list_->add_plugin(plugin1);
  list_->add_plugin(plugin2);
  list_->register_listener(1, "shared_channel,own_channel");
  list_->register_listener(2, "shared_channel");

  list_->unregister_listener(1, "SHARED_CHANNEL");

  const auto shared = list_->get("shared_channel");
  ASSERT_EQ(shared.size(), 1u);
  EXPECT_EQ(shared.front()->get_id(), 2u);
  EXPECT_EQ(list_->get("own_channel").size(), 1u);

  list_->unregister_listener(2, "shared_channel");
  EXPECT_EQ(list_->get_listeners().count("shared_channel"), 0u);
  // Unregistering what was never registered is harmless.
  EXPECT_NO_THROW(list_->unregister_listener(1, "never_registered"));
}

TEST_F(PluginsListWithListenerTest, GetSkipsSubscriberIdsWithNoLoadedPlugin) {
  // A listener id with no matching plugin must be skipped. This used to be
  // `plugins_[id]`, whose default-insert both wrote to the map under a shared
  // lock and handed the caller a null plugin to dereference.
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);
  list_->register_listener(1, "channel");
  list_->listeners_["channel"].insert(999);

  const std::size_t before = list_->plugins_.size();
  const auto listeners = list_->get("channel");

  ASSERT_EQ(listeners.size(), 1u);
  EXPECT_EQ(listeners.front()->get_id(), 1u);
  EXPECT_NE(listeners.front(), nullptr);
  // The lookup must not have grown the plugin map.
  EXPECT_EQ(list_->plugins_.size(), before);
  EXPECT_TRUE(list_->plugins_.find(999) == list_->plugins_.end());
}

TEST_F(PluginsListWithListenerTest, RemoveAllClearsListeners) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);
  list_->register_listener(1, "channel");

  list_->remove_all();

  const auto plugins = list_->list();
  EXPECT_TRUE(plugins.empty());
}

TEST_F(PluginsListWithListenerTest, ChannelNameIsCaseInsensitive) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "Module");
  list_->add_plugin(plugin);

  list_->register_listener(1, "TestChannel");

  // Should find with lowercase
  const auto listeners = list_->get("testchannel");
  EXPECT_EQ(listeners.size(), 1u);
}

TEST_F(PluginsListWithListenerTest, ToStringWithNoPlugins) {
  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("NONE") != std::string::npos);
}

TEST_F(PluginsListWithListenerTest, ToStringWithPlugins) {
  const auto plugin = std::make_shared<MockListPlugin>(1, "alias", "TestModule");
  list_->add_plugin(plugin);
  list_->register_listener(1, "channel");

  const std::string result = list_->to_string();
  EXPECT_TRUE(result.find("channel") != std::string::npos);
}

TEST_F(PluginsListWithListenerTest, HavePluginReturnsTrueForExisting) {
  const auto plugin = std::make_shared<MockListPlugin>(42, "alias", "Module");
  list_->add_plugin(plugin);

  EXPECT_TRUE(list_->have_plugin(42));
}

TEST_F(PluginsListWithListenerTest, HavePluginReturnsFalseForNonExisting) { EXPECT_FALSE(list_->have_plugin(999)); }

// ============================================================================
// plugins_list_listeners_impl tests
// ============================================================================

TEST(PluginsListListenersImplTest, RemoveAllClearsListeners) {
  nsclient::plugins_list_listeners_impl impl;

  // Add some data to listeners_
  impl.listeners_["channel1"].insert(1);
  impl.listeners_["channel2"].insert(2);

  impl.remove_all();

  EXPECT_TRUE(impl.listeners_.empty());
}

TEST(PluginsListListenersImplTest, RemovePluginRemovesFromAllChannels) {
  nsclient::plugins_list_listeners_impl impl;

  impl.listeners_["channel1"].insert(1);
  impl.listeners_["channel1"].insert(2);
  impl.listeners_["channel2"].insert(1);

  impl.remove_plugin(1);

  // channel2 had only plugin 1, so the entry goes.
  EXPECT_TRUE(impl.listeners_.find("channel2") == impl.listeners_.end());
  // channel1 keeps plugin 2 - removing one subscriber must not unsubscribe the
  // others, which is what erasing the whole entry used to do.
  const auto ch1 = impl.listeners_.find("channel1");
  ASSERT_TRUE(ch1 != impl.listeners_.end());
  EXPECT_EQ(ch1->second.size(), 1u);
  EXPECT_TRUE(ch1->second.count(2) > 0);
  EXPECT_TRUE(ch1->second.count(1) == 0);
}

TEST(PluginsListListenersImplTest, RemovePluginLeavesNoStaleId) {
  // The id must be gone from every channel, not just from the ones whose entry
  // happened to be erased.
  nsclient::plugins_list_listeners_impl impl;

  impl.listeners_["a"].insert(1);
  impl.listeners_["a"].insert(2);
  impl.listeners_["b"].insert(1);
  impl.listeners_["b"].insert(2);

  impl.remove_plugin(1);

  for (const auto& entry : impl.listeners_) {
    EXPECT_TRUE(entry.second.count(1) == 0) << "stale id left in channel " << entry.first;
  }
}

TEST(PluginsListListenersImplTest, RemoveUnknownPluginIsANoOp) {
  nsclient::plugins_list_listeners_impl impl;

  impl.listeners_["channel"].insert(1);

  impl.remove_plugin(999);

  const auto it = impl.listeners_.find("channel");
  ASSERT_TRUE(it != impl.listeners_.end());
  EXPECT_EQ(it->second.size(), 1u);
}

TEST(PluginsListListenersImplTest, ListReturnsAllChannels) {
  nsclient::plugins_list_listeners_impl impl;

  impl.listeners_["channel1"].insert(1);
  impl.listeners_["channel2"].insert(2);

  std::list<std::string> channels;
  impl.list(channels);

  EXPECT_EQ(channels.size(), 2u);
}

TEST(PluginsListListenersImplTest, ToStringWithNoListeners) {
  nsclient::plugins_list_listeners_impl impl;

  const std::string result = impl.to_string();
  EXPECT_TRUE(result.find("NONE") != std::string::npos);
}

TEST(PluginsListListenersImplTest, ToStringWithListeners) {
  nsclient::plugins_list_listeners_impl impl;

  impl.listeners_["test_channel"].insert(1);

  const std::string result = impl.to_string();
  EXPECT_TRUE(result.find("test_channel") != std::string::npos);
}
