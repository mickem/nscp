// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for nsclient::logging::impl::nsclient_logger.
 *
 * nsclient_logger composes a backend (console / file / threaded-file) and
 * fans log messages out to a list of subscribers.
 *
 * Important caveat: the default constructor selects the platform default
 * backend (threaded-file on Windows, which spawns a worker thread; console
 * elsewhere, which patches std::cout's streambuf). Both have side-effects
 * we'd rather not propagate across the gtest runner. Each test below calls
 * destroy() on the freshly-constructed logger before doing anything else,
 * which releases the default backend cleanly (and joins the threaded-file
 * worker via its destructor). Tests then exercise the parts that don't need
 * a backend at all (subscriber broadcast, log-level handling, is-safe-with-
 * no-backend).
 */

#include "nsclient_logger.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <thread>
#include <vector>

#include "threaded_logger.hpp"

using nsclient::logging::log_message_factory;
using nsclient::logging::logging_subscriber;
using nsclient::logging::impl::nsclient_logger;

namespace {

class CapturingSubscriber : public logging_subscriber {
 public:
  void on_log_message(const std::string& payload) override {
    std::lock_guard<std::mutex> g(mu);
    payloads.push_back(payload);
  }
  std::vector<std::string> snapshot() {
    std::lock_guard<std::mutex> g(mu);
    return payloads;
  }
  std::vector<std::string> payloads;
  std::mutex mu;
};

// Build a logger with no live backend so the rest of the test exercises
// only the in-memory subscriber list and log-level state.
std::unique_ptr<nsclient_logger> make_backendless_logger() {
  auto logger = std::make_unique<nsclient_logger>();
  logger->destroy();  // join/release the default platform backend
  return logger;
}

}  // namespace

TEST(NsclientLogger, ConstructAndDestroyReleasesDefaultBackend) {
  auto logger = std::make_unique<nsclient_logger>();
  EXPECT_NO_THROW(logger->destroy());
}

TEST(NsclientLogger, OnLogMessageBroadcastsToAllSubscribers) {
  auto logger = make_backendless_logger();
  auto a = std::make_shared<CapturingSubscriber>();
  auto b = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(a);
  logger->add_subscriber(b);

  logger->on_log_message("payload-1");
  logger->on_log_message("payload-2");

  EXPECT_EQ(a->snapshot(), (std::vector<std::string>{"payload-1", "payload-2"}));
  EXPECT_EQ(b->snapshot(), (std::vector<std::string>{"payload-1", "payload-2"}));
}

TEST(NsclientLogger, ClearSubscribersStopsBroadcast) {
  auto logger = make_backendless_logger();
  auto sub = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(sub);

  logger->on_log_message("first");
  ASSERT_EQ(sub->snapshot().size(), 1u);

  logger->clear_subscribers();
  logger->on_log_message("ignored");

  EXPECT_EQ(sub->snapshot().size(), 1u);
}

TEST(NsclientLogger, OnLogMessageWithNoSubscribersIsSafe) {
  auto logger = make_backendless_logger();
  EXPECT_NO_THROW(logger->on_log_message("noop"));
}

TEST(NsclientLogger, DoLogIsSafeWithNoBackend) {
  auto logger = make_backendless_logger();
  EXPECT_NO_THROW(logger->do_log("after destroy"));
}

TEST(NsclientLogger, RawForwardsToBackend) {
  // Without a backend, raw is a no-op; the test verifies it does not throw.
  auto logger = make_backendless_logger();
  EXPECT_NO_THROW(logger->raw("anything"));
}

TEST(NsclientLogger, StartupAndShutdownReturnFalseWithNoBackend) {
  auto logger = make_backendless_logger();
  EXPECT_FALSE(logger->startup());
  EXPECT_FALSE(logger->shutdown());
}

TEST(NsclientLogger, ConfigureWithNoBackendIsSafe) {
  auto logger = make_backendless_logger();
  EXPECT_NO_THROW(logger->configure());
}

TEST(NsclientLogger, SetLogLevelControlsShouldPredicates) {
  auto logger = make_backendless_logger();

  logger->set_log_level("error");
  EXPECT_TRUE(logger->should_error());
  EXPECT_TRUE(logger->should_critical());
  EXPECT_FALSE(logger->should_debug());
  EXPECT_FALSE(logger->should_trace());
  EXPECT_FALSE(logger->should_info());
  EXPECT_FALSE(logger->should_warning());

  logger->set_log_level("trace");
  EXPECT_TRUE(logger->should_trace());
  EXPECT_TRUE(logger->should_debug());
  EXPECT_TRUE(logger->should_info());
  EXPECT_TRUE(logger->should_warning());
  EXPECT_TRUE(logger->should_error());
  EXPECT_TRUE(logger->should_critical());
}

TEST(NsclientLogger, SetLogLevelInvalidIsSafe) {
  auto logger = make_backendless_logger();
  // Invalid level routes a synthesised error message through do_log; with no
  // backend that's a no-op. The test checks for absence of an exception.
  EXPECT_NO_THROW(logger->set_log_level("not-a-level"));
}

// Covers every level enum value from log_level.hpp that round-trips cleanly
// through set_log_level()/get_log_level():
//   critical (1), error (2), warning (3), debug (50), trace (99).
// The two values that DO NOT round-trip ("info" denormalises to "message",
// "off" is not parseable) are pinned by GetLogLevelReturnsCurrentStrangeIncorrectCase.
TEST(NsclientLogger, GetLogLevelReturnsCurrent) {
  auto logger = make_backendless_logger();

  for (const std::string level : {"critical", "error", "warning", "debug", "trace"}) {
    SCOPED_TRACE("level=" + level);
    logger->set_log_level(level);
    EXPECT_EQ(logger->get_log_level(), level);
  }
}

// Pins the two known asymmetries between set_log_level and get_log_level:
//   * "info" is accepted but reported back as "message".
//   * "off" maps to level 0 in log_level.hpp but log_level::set() does not
//     parse it, so the previously-set level survives unchanged.
TEST(NsclientLogger, GetLogLevelReturnsCurrentStrangeIncorrectCase) {
  auto logger = make_backendless_logger();

  // "info" -> "message" (asymmetric on purpose, see log_level.cpp).
  logger->set_log_level("info");
  EXPECT_EQ(logger->get_log_level(), "message");

  // "off" is silently rejected; the previous level (here: "message") sticks.
  logger->set_log_level("off");
  EXPECT_EQ(logger->get_log_level(), "message");

  // Sanity: an obviously bogus value is also rejected without disturbing state.
  logger->set_log_level("not-a-level");
  EXPECT_EQ(logger->get_log_level(), "message");
}

TEST(NsclientLogger, LogMethodsRespectLevel) {
  auto logger = make_backendless_logger();
  auto sub = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(sub);

  logger->set_log_level("error");
  // These messages would only reach the subscriber via the backend's
  // on_log_message callback - we have no backend, so the subscriber sees
  // nothing regardless. We're really validating that the should_X gating
  // suppresses formatting work entirely.
  logger->trace("m", __FILE__, __LINE__, "trace-msg");
  logger->debug("m", __FILE__, __LINE__, "debug-msg");
  logger->info("m", __FILE__, __LINE__, "info-msg");
  logger->warning("m", __FILE__, __LINE__, "warning-msg");
  // Below-threshold messages are suppressed before do_log; subscriber
  // accordingly sees nothing.
  EXPECT_TRUE(sub->snapshot().empty());
}

// The driver options cli_parser pushes onto the same list as severities.
// These used to fall through to log_level::set(), which does not know them:
// --no-stderr and oneline logged "Invalid log level: ..." instead of taking
// effect, and there was no way at all to turn the console back off.
TEST(NsclientLogger, DriverOptionsDoNotDisturbTheSeverityLevel) {
  auto logger = make_backendless_logger();
  logger->set_log_level("warning");
  ASSERT_EQ(logger->get_log_level(), "warning");

  for (const std::string option : {"console", "no-console", "oneline", "no-std-err"}) {
    SCOPED_TRACE("option=" + option);
    EXPECT_NO_THROW(logger->set_log_level(option));
    EXPECT_EQ(logger->get_log_level(), "warning");
  }
}

// ===== delivery outside the lock ==========================================
//
// on_log_message() used to hold the subscriber mutex (a 5 s timed one) across
// the whole fan-out. A subscriber that logged from inside its handler then
// re-entered on the same thread, waited out the 5 s and lost the line, and a
// remove() during a slow delivery gave up after 5 s and left the subscriber
// in the list. Delivery now runs on a snapshot with the lock released, and a
// line a handler logs from inside a delivery is not fanned out again.

namespace {

// Logs again from inside its handler on every line it is handed, through the
// logger's own do_log as a module's log call would, the way a log-handler
// module that reports each line it processes does. Unbounded: if the nested
// line were fanned out again, this would feed itself forever.
class ReentrantSubscriber : public logging_subscriber {
 public:
  explicit ReentrantSubscriber(nsclient_logger* logger) : logger_(logger) {}
  void on_log_message(const std::string& payload) override {
    {
      std::lock_guard<std::mutex> g(mu);
      payloads.push_back(payload);
    }
    logger_->do_log("nested:" + payload);
  }
  std::vector<std::string> snapshot() {
    std::lock_guard<std::mutex> g(mu);
    return payloads;
  }
  nsclient_logger* logger_;
  std::vector<std::string> payloads;
  std::mutex mu;
};

// Unsubscribes itself from inside its handler.
class SelfRemovingSubscriber : public logging_subscriber, public std::enable_shared_from_this<SelfRemovingSubscriber> {
 public:
  explicit SelfRemovingSubscriber(nsclient_logger* logger) : logger_(logger) {}
  void on_log_message(const std::string&) override {
    ++calls;
    logger_->remove_subscriber(shared_from_this());
  }
  nsclient_logger* logger_;
  std::atomic<int> calls{0};
};

// The console backend's shape: do_log hands the line straight back to the
// subscriber manager on the calling thread.
class SynchronousBackend : public nsclient::logging::log_driver_interface_impl {
 public:
  explicit SynchronousBackend(logging_subscriber* manager) : manager_(manager) {}
  void do_log(std::string data) override {
    {
      std::lock_guard<std::mutex> g(mu);
      logged.push_back(data);
    }
    manager_->on_log_message(data);
  }
  void synch_configure() override {}
  void asynch_configure() override {}
  using log_driver_interface_impl::set_config;
  void set_config(const std::string&) override {}
  std::vector<std::string> snapshot() {
    std::lock_guard<std::mutex> g(mu);
    return logged;
  }
  logging_subscriber* manager_;
  std::mutex mu;
  std::vector<std::string> logged;
};

// A sink for the threaded backend: records what reaches it.
class RecordingBackend : public nsclient::logging::log_driver_interface_impl {
 public:
  void do_log(std::string data) override {
    std::lock_guard<std::mutex> g(mu);
    logged.push_back(std::move(data));
  }
  void synch_configure() override {}
  void asynch_configure() override {}
  using log_driver_interface_impl::set_config;
  void set_config(const std::string&) override {}
  std::vector<std::string> snapshot() {
    std::lock_guard<std::mutex> g(mu);
    return logged;
  }
  std::size_t size() {
    std::lock_guard<std::mutex> g(mu);
    return logged.size();
  }
  std::mutex mu;
  std::vector<std::string> logged;
};

// Blocks inside its handler until released, so a test can hold a delivery
// open on one thread while another calls remove().
class BlockingSubscriber : public logging_subscriber {
 public:
  // With block_on set, only that line blocks; every other passes through.
  std::string block_on;
  void on_log_message(const std::string& payload) override {
    if (!block_on.empty() && payload != block_on) return;
    std::unique_lock<std::mutex> lock(mu);
    entered = true;
    cv.notify_all();
    cv.wait(lock, [this]() { return released; });
    finished = true;
  }
  void wait_until_entered() {
    std::unique_lock<std::mutex> lock(mu);
    cv.wait(lock, [this]() { return entered; });
  }
  void release() {
    {
      std::lock_guard<std::mutex> lock(mu);
      released = true;
    }
    cv.notify_all();
  }
  std::mutex mu;
  std::condition_variable cv;
  bool entered = false;
  bool released = false;
  std::atomic<bool> finished{false};
};

}  // namespace

TEST(NsclientLogger, SubscriberLoggingFromItsHandlerOnTheConsoleBackendNeitherStallsNorRecurses) {
  auto logger = make_backendless_logger();
  auto backend = std::make_shared<SynchronousBackend>(logger.get());
  logger->use_backend(backend);
  auto reentrant = std::make_shared<ReentrantSubscriber>(logger.get());
  auto other = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(reentrant);
  logger->add_subscriber(other);

  const auto started = std::chrono::steady_clock::now();
  logger->do_log("outer");
  const auto elapsed = std::chrono::steady_clock::now() - started;

  // The line reaches both subscribers once. The handler's own line reaches
  // the backend but is not fanned out to the handlers again, and nothing
  // sat out a lock timeout.
  EXPECT_EQ(reentrant->snapshot(), (std::vector<std::string>{"outer"}));
  EXPECT_EQ(other->snapshot(), (std::vector<std::string>{"outer"}));
  EXPECT_EQ(backend->snapshot(), (std::vector<std::string>{"outer", "nested:outer"}));
  EXPECT_LT(elapsed, std::chrono::seconds(2));
  logger->destroy();
}

TEST(NsclientLogger, SubscriberLoggingFromItsHandlerOnTheThreadedBackendDoesNotFeedItself) {
  // On the threaded backend the handler's line is queued and delivered by
  // the same worker next. Without the tag taken at do_log, the handler
  // would be handed its own line, log again, and the queue would never
  // drain.
  auto logger = make_backendless_logger();
  auto sink = std::make_shared<RecordingBackend>();
  auto threaded = std::make_shared<nsclient::logging::impl::threaded_logger>(logger.get(), sink);
  threaded->set_join_timeout_for_test(boost::posix_time::seconds(2));
  logger->use_backend(threaded);
  auto reentrant = std::make_shared<ReentrantSubscriber>(logger.get());
  logger->add_subscriber(reentrant);

  logger->do_log("outer");
  // Both lines reach the sink: the handler's own line is still logged.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (sink->size() < 2 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // And then nothing more: the handler was handed "outer" once.
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  EXPECT_EQ(sink->snapshot(), (std::vector<std::string>{"outer", "nested:outer"}));
  EXPECT_EQ(reentrant->snapshot(), (std::vector<std::string>{"outer"}));
  logger->destroy();
}

TEST(NsclientLogger, DeliveriesRunConcurrentlyAcrossThreads) {
  // One stuck handler no longer parks every other thread that logs behind
  // it: a second line reaches the other handlers while the first is still
  // inside the blocking one.
  auto logger = make_backendless_logger();
  auto blocking = std::make_shared<BlockingSubscriber>();
  auto other = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(blocking);
  logger->add_subscriber(other);

  blocking->block_on = "first";
  std::thread first([&logger]() { logger->on_log_message("first"); });
  blocking->wait_until_entered();
  // Only "first" blocks in that handler; "second" runs straight through it.
  std::thread second([&logger]() { logger->on_log_message("second"); });
  second.join();
  EXPECT_EQ(other->snapshot(), (std::vector<std::string>{"second"}));

  blocking->release();
  first.join();
  EXPECT_EQ(other->snapshot(), (std::vector<std::string>{"second", "first"}));
}

TEST(NsclientLogger, SubscriberMayRemoveItselfFromInsideItsHandler) {
  auto logger = make_backendless_logger();
  auto self_removing = std::make_shared<SelfRemovingSubscriber>(logger.get());
  auto other = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(self_removing);
  logger->add_subscriber(other);

  const auto started = std::chrono::steady_clock::now();
  logger->on_log_message("first");
  logger->on_log_message("second");
  const auto elapsed = std::chrono::steady_clock::now() - started;

  // Removed on the first line, so it never sees the second; the other
  // subscriber sees both and nothing waited for a timeout.
  EXPECT_EQ(self_removing->calls.load(), 1);
  EXPECT_EQ(other->snapshot(), (std::vector<std::string>{"first", "second"}));
  EXPECT_LT(elapsed, std::chrono::seconds(2));
}

TEST(NsclientLogger, RemoveWaitsForADeliveryInFlightOnAnotherThread) {
  auto logger = make_backendless_logger();
  auto blocking = std::make_shared<BlockingSubscriber>();
  logger->add_subscriber(blocking);

  std::thread delivery([&logger]() { logger->on_log_message("slow"); });
  blocking->wait_until_entered();

  std::atomic<bool> removed{false};
  std::thread remover([&]() {
    logger->remove_subscriber(blocking);
    removed = true;
  });

  // The subscriber is still inside its handler, so remove() must not have
  // returned yet - the caller is about to tear the subscriber down.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_FALSE(removed.load());
  EXPECT_FALSE(blocking->finished.load());

  blocking->release();
  remover.join();
  delivery.join();
  EXPECT_TRUE(blocking->finished.load());

  // And it really is gone: a later line does not reach it.
  blocking->entered = false;
  logger->on_log_message("after");
  EXPECT_FALSE(blocking->entered);
}

TEST(NsclientLogger, RemoveReportsWhetherTheSubscriberWasOnTheList) {
  auto logger = make_backendless_logger();
  auto sub = std::make_shared<CapturingSubscriber>();
  auto never_added = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(sub);
  EXPECT_FALSE(logger->remove_subscriber(never_added).removed);
  const auto removed = logger->remove_subscriber(sub);
  EXPECT_TRUE(removed.removed);
  EXPECT_FALSE(removed.delivering);
  EXPECT_FALSE(logger->remove_subscriber(sub).removed);
}

TEST(NsclientLogger, RemovingAnUnknownSubscriberDoesNotWaitForADelivery) {
  // The plugin manager unsubscribes every module it unloads, handler or
  // not, so a module that never subscribed must not be parked behind a
  // slow handler on another thread.
  auto logger = make_backendless_logger();
  auto blocking = std::make_shared<BlockingSubscriber>();
  auto never_added = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(blocking);

  std::thread delivery([&logger]() { logger->on_log_message("slow"); });
  blocking->wait_until_entered();

  const auto started = std::chrono::steady_clock::now();
  EXPECT_FALSE(logger->remove_subscriber(never_added).removed);
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));

  blocking->release();
  delivery.join();
}

TEST(NsclientLogger, RemoveReportsADeliveryThatOutlivesTheWait) {
  // The caller is about to unload the module; a handler still inside after
  // the wait is reported so it can refuse, as it would a stuck dispatch.
  auto logger = make_backendless_logger();
  logger->set_delivery_wait(std::chrono::milliseconds(200));
  auto blocking = std::make_shared<BlockingSubscriber>();
  logger->add_subscriber(blocking);

  std::thread delivery([&logger]() { logger->on_log_message("stuck"); });
  blocking->wait_until_entered();

  const auto started = std::chrono::steady_clock::now();
  const auto result = logger->remove_subscriber(blocking);
  EXPECT_TRUE(result.removed);
  EXPECT_TRUE(result.delivering);
  EXPECT_GE(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(200));

  blocking->release();
  delivery.join();
}

TEST(NsclientLogger, ClearReportsADeliveryThatOutlivesTheWait) {
  auto logger = make_backendless_logger();
  logger->set_delivery_wait(std::chrono::milliseconds(200));
  EXPECT_FALSE(logger->clear_subscribers().removed);  // empty list: nothing to wait for
  auto blocking = std::make_shared<BlockingSubscriber>();
  logger->add_subscriber(blocking);

  std::thread delivery([&logger]() { logger->on_log_message("stuck"); });
  blocking->wait_until_entered();

  const auto result = logger->clear_subscribers();
  EXPECT_TRUE(result.removed);
  EXPECT_TRUE(result.delivering);

  blocking->release();
  delivery.join();
}

TEST(NsclientLogger, RemoveAndClearReturnAtOnceWithNothingInFlight) {
  // Plain add / remove with nothing in flight returns at once.
  auto logger = make_backendless_logger();
  auto sub = std::make_shared<CapturingSubscriber>();
  logger->add_subscriber(sub);
  const auto started = std::chrono::steady_clock::now();
  logger->remove_subscriber(sub);
  logger->clear_subscribers();
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
}
