// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for nsclient::logging::impl::threaded_logger.
 *
 * threaded_logger is a fan-out / queue-based wrapper around a "background"
 * log_driver_interface. Messages pushed via do_log() are processed by an
 * internal thread which forwards each entry to the background logger and
 * notifies a logging_subscriber.
 *
 * These tests use fake implementations of both collaborators so the wrapper
 * can be exercised in isolation, without touching settings, the filesystem
 * or stdout.
 */

#include "threaded_logger.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <nsclient/logger/log_driver_interface.hpp>
#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <thread>
#include <vector>

using nsclient::logging::log_driver_instance;
using nsclient::logging::log_driver_interface;
using nsclient::logging::log_driver_interface_impl;
using nsclient::logging::logging_subscriber;
using nsclient::logging::impl::threaded_logger;

namespace {

class FakeSubscriber : public logging_subscriber {
 public:
  void on_log_message(const std::string& payload) override {
    std::lock_guard<std::mutex> g(mu);
    messages.push_back(payload);
  }
  std::vector<std::string> snapshot() {
    std::lock_guard<std::mutex> g(mu);
    return messages;
  }
  std::mutex mu;
  std::vector<std::string> messages;
};

class FakeBackend : public log_driver_interface_impl {
 public:
  void do_log(std::string data) override {
    std::lock_guard<std::mutex> g(mu);
    logged.push_back(std::move(data));
  }
  void synch_configure() override { ++synch_configure_count; }
  void asynch_configure() override { ++asynch_configure_count; }
  void set_config(const std::string& key) override {
    std::lock_guard<std::mutex> g(mu);
    set_config_keys.push_back(key);
  }
  // Inherit set_config(log_driver_instance) from impl.
  using log_driver_interface_impl::set_config;

  std::vector<std::string> snapshot() {
    std::lock_guard<std::mutex> g(mu);
    return logged;
  }
  std::vector<std::string> set_config_snapshot() {
    std::lock_guard<std::mutex> g(mu);
    return set_config_keys;
  }

  std::mutex mu;
  std::vector<std::string> logged;
  std::vector<std::string> set_config_keys;
  std::atomic<int> synch_configure_count{0};
  std::atomic<int> asynch_configure_count{0};
};

// Wait until `pred` returns true or `timeout` elapses. Returns true on success.
template <typename Pred>
bool wait_for(Pred pred, std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return pred();
}

}  // namespace

TEST(ThreadedLogger, StartupSpawnsThreadAndShutdownJoins) {
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  threaded_logger tl(&sub, backend);
  EXPECT_FALSE(tl.is_started());
  EXPECT_TRUE(tl.startup());
  EXPECT_TRUE(tl.is_started());
  EXPECT_TRUE(tl.shutdown());
  EXPECT_FALSE(tl.is_started());
}

TEST(ThreadedLogger, StartupIsIdempotent) {
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  threaded_logger tl(&sub, backend);
  EXPECT_TRUE(tl.startup());
  EXPECT_TRUE(tl.startup());  // second call is a no-op
  EXPECT_TRUE(tl.shutdown());
}

TEST(ThreadedLogger, ShutdownWithoutStartupIsNoOp) {
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  threaded_logger tl(&sub, backend);
  EXPECT_TRUE(tl.shutdown());
}

TEST(ThreadedLogger, MessagesAreForwardedToBackendAndSubscriber) {
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  threaded_logger tl(&sub, backend);
  ASSERT_TRUE(tl.startup());

  tl.do_log("first");
  tl.do_log("second");
  tl.do_log("third");

  ASSERT_TRUE(wait_for([&] { return backend->snapshot().size() >= 3 && sub.snapshot().size() >= 3; }));
  tl.shutdown();

  EXPECT_EQ(backend->snapshot(), (std::vector<std::string>{"first", "second", "third"}));
  EXPECT_EQ(sub.snapshot(), (std::vector<std::string>{"first", "second", "third"}));
}

TEST(ThreadedLogger, AsynchConfigureForwardsToBackend) {
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  threaded_logger tl(&sub, backend);
  ASSERT_TRUE(tl.startup());

  tl.asynch_configure();
  ASSERT_TRUE(wait_for([&] { return backend->asynch_configure_count.load() >= 1; }));

  tl.shutdown();
  EXPECT_EQ(backend->asynch_configure_count.load(), 1);
  EXPECT_TRUE(backend->snapshot().empty()) << "configure must not be forwarded as a log message";
}

TEST(ThreadedLogger, SynchConfigureRunsInline) {
  // synch_configure is intentionally synchronous: it does NOT go through the
  // queue, it forwards directly to the backend.
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  threaded_logger tl(&sub, backend);
  // No startup() needed because synch_configure bypasses the queue.

  tl.synch_configure();
  EXPECT_EQ(backend->synch_configure_count.load(), 1);
}

TEST(ThreadedLogger, SetConfigKeyForwardedToBackend) {
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  threaded_logger tl(&sub, backend);
  ASSERT_TRUE(tl.startup());

  tl.set_config("oneline");
  tl.set_config("console");
  ASSERT_TRUE(wait_for([&] { return backend->set_config_snapshot().size() >= 2; }));

  tl.shutdown();
  EXPECT_EQ(backend->set_config_snapshot(), (std::vector<std::string>{"oneline", "console"}));
  EXPECT_TRUE(backend->snapshot().empty()) << "set_config must not be forwarded as a log message";
}

TEST(ThreadedLogger, MessagesProcessedInOrder) {
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  threaded_logger tl(&sub, backend);
  ASSERT_TRUE(tl.startup());

  for (int i = 0; i < 100; ++i) {
    tl.do_log("msg-" + std::to_string(i));
  }
  ASSERT_TRUE(wait_for([&] { return backend->snapshot().size() == 100; }));
  tl.shutdown();

  const auto out = backend->snapshot();
  ASSERT_EQ(out.size(), 100u);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(out[static_cast<size_t>(i)], "msg-" + std::to_string(i));
  }
}

TEST(ThreadedLogger, DestructorJoinsBackgroundThread) {
  // Ensures that not calling shutdown() explicitly still results in a clean
  // teardown (the destructor calls shutdown). Test passes by not hanging /
  // crashing.
  FakeSubscriber sub;
  auto backend = std::make_shared<FakeBackend>();
  {
    threaded_logger tl(&sub, backend);
    ASSERT_TRUE(tl.startup());
    tl.do_log("hello");
    ASSERT_TRUE(wait_for([&] { return backend->snapshot().size() >= 1; }));
  }
  EXPECT_EQ(backend->snapshot().size(), 1u);
}

// A backend whose do_log() parks until released, so shutdown()'s join times
// out and takes the abandon-and-detach path.
namespace {
class BlockingBackend : public log_driver_interface_impl {
 public:
  void do_log(std::string) override {
    entered = true;
    while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  void synch_configure() override {}
  void asynch_configure() override {}
  void set_config(const std::string &) override {}
  using log_driver_interface_impl::set_config;

  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
};
}  // namespace

// The worker that shutdown() gives up on is detached and outlives the
// threaded_logger. It must not call back into the subscriber afterwards: the
// subscriber manager is the nsclient_logger that owns the logger, so it is
// being destroyed right behind us. Under a sanitizer this is the test that
// fails if thread_proc ever reaches back into the logger again.
TEST(ThreadedLogger, AbandonedWorkerDoesNotTouchSubscriberAfterShutdown) {
  auto sub = std::make_shared<FakeSubscriber>();
  auto backend = std::make_shared<BlockingBackend>();
  {
    threaded_logger tl(sub.get(), backend);
    tl.set_join_timeout_for_test(boost::posix_time::milliseconds(100));
    tl.startup();
    tl.do_log("parked");
    ASSERT_TRUE(wait_for([&] { return backend->entered.load(); }));
    // The worker is stuck inside the backend, so the join times out and
    // shutdown reports failure rather than pretending it stopped.
    EXPECT_FALSE(tl.shutdown());
  }
  // The logger is gone. Release the worker: it finishes the do_log it was in
  // and must then find the subscriber cleared instead of calling it.
  backend->release = true;
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_TRUE(sub->snapshot().empty());
}

// The background logger is reference counted by the shared state, so an
// abandoned worker still has a live sink to finish writing to.
TEST(ThreadedLogger, AbandonedWorkerKeepsBackendAlive) {
  auto sub = std::make_shared<FakeSubscriber>();
  auto backend = std::make_shared<BlockingBackend>();
  const long use_count_before = backend.use_count();
  {
    threaded_logger tl(sub.get(), backend);
    tl.set_join_timeout_for_test(boost::posix_time::milliseconds(100));
    tl.startup();
    tl.do_log("parked");
    ASSERT_TRUE(wait_for([&] { return backend->entered.load(); }));
    EXPECT_FALSE(tl.shutdown());
  }
  // Still held by the detached worker's shared_state, not just by us.
  EXPECT_GT(backend.use_count(), use_count_before);
  backend->release = true;
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
}
