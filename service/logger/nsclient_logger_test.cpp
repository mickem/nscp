// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for nsclient::logging::impl::nsclient_logger: level and console
 * options, and the subscriber fan-out on its one delivery thread. A fresh
 * logger has the console off and no log file, so nothing here writes
 * anywhere but to the subscribers.
 */

#include "nsclient_logger.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <nscapi/protobuf/log.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <thread>
#include <vector>

using nsclient::logging::logging_subscriber;
using nsclient::logging::impl::nsclient_logger;

namespace {

// A serialized LogEntry carrying one message, the shape every real line has.
std::string line(const std::string& message) {
  PB::Log::LogEntry entry;
  entry.add_entry()->set_message(message);
  return entry.SerializeAsString();
}
std::string message_of(const std::string& data) {
  PB::Log::LogEntry entry;
  if (!entry.ParseFromString(data) || entry.entry_size() == 0) return "<not a LogEntry>";
  return entry.entry(0).message();
}

class CapturingSubscriber : public logging_subscriber {
 public:
  void on_log_message(const std::string& payload) override {
    std::lock_guard<std::mutex> g(mu);
    payloads.push_back(message_of(payload));
    cv.notify_all();
  }
  std::vector<std::string> snapshot() {
    std::lock_guard<std::mutex> g(mu);
    return payloads;
  }
  // Delivery is asynchronous: wait for `n` lines (or give up after 5 s).
  std::vector<std::string> wait_for(std::size_t n) {
    std::unique_lock<std::mutex> g(mu);
    cv.wait_for(g, std::chrono::seconds(5), [&]() { return payloads.size() >= n; });
    return payloads;
  }
  std::vector<std::string> payloads;
  std::mutex mu;
  std::condition_variable cv;
};

// Logs again from inside its handler on every line it is handed, through the
// logger's do_log as a module's log call would. Unbounded: if the nested line
// were handed out again, this would feed itself forever.
class ReentrantSubscriber : public CapturingSubscriber {
 public:
  explicit ReentrantSubscriber(nsclient_logger* logger) : logger_(logger) {}
  void on_log_message(const std::string& payload) override {
    CapturingSubscriber::on_log_message(payload);
    logger_->do_log(line("nested"));
  }
  nsclient_logger* logger_;
};

// Unsubscribes itself from inside its handler.
class SelfRemovingSubscriber : public logging_subscriber, public std::enable_shared_from_this<SelfRemovingSubscriber> {
 public:
  explicit SelfRemovingSubscriber(nsclient_logger* logger) : logger_(logger) {}
  void on_log_message(const std::string&) override {
    removed = logger_->remove_subscriber(shared_from_this()).removed;
    ++calls;
  }
  nsclient_logger* logger_;
  std::atomic<bool> removed{false};
  std::atomic<int> calls{0};
};

// Blocks inside its handler until released.
class BlockingSubscriber : public logging_subscriber {
 public:
  void on_log_message(const std::string&) override {
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

// ===== levels and console options ==========================================

TEST(NsclientLogger, ConstructAndDestroy) {
  auto logger = std::make_unique<nsclient_logger>();
  EXPECT_NO_THROW(logger->destroy());
  EXPECT_NO_THROW(logger->do_log(line("after destroy")));
}

TEST(NsclientLogger, StartupAndShutdownSucceed) {
  nsclient_logger logger;
  EXPECT_TRUE(logger.startup());
  EXPECT_TRUE(logger.shutdown());
}

TEST(NsclientLogger, ConfigureWithoutSettingsIsSafe) {
  nsclient_logger logger;
  EXPECT_NO_THROW(logger.configure());
  logger.set_backend("file");
  EXPECT_NO_THROW(logger.configure());
}

TEST(NsclientLogger, SetLogLevelControlsShouldPredicates) {
  nsclient_logger logger;
  logger.set_log_level("error");
  EXPECT_TRUE(logger.should_error());
  EXPECT_TRUE(logger.should_critical());
  EXPECT_FALSE(logger.should_warning());
  EXPECT_FALSE(logger.should_debug());

  logger.set_log_level("trace");
  EXPECT_TRUE(logger.should_trace());
  EXPECT_TRUE(logger.should_info());
}

TEST(NsclientLogger, GetLogLevelReturnsCurrent) {
  nsclient_logger logger;
  for (const std::string level : {"critical", "error", "warning", "debug", "trace"}) {
    SCOPED_TRACE("level=" + level);
    logger.set_log_level(level);
    EXPECT_EQ(logger.get_log_level(), level);
  }
  // "info" is reported back as "message"; "off" and junk leave it alone.
  logger.set_log_level("info");
  EXPECT_EQ(logger.get_log_level(), "message");
  logger.set_log_level("off");
  logger.set_log_level("not-a-level");
  EXPECT_EQ(logger.get_log_level(), "message");
}

// The console options cli_parser pushes onto the same list as severities.
TEST(NsclientLogger, ConsoleOptionsDoNotDisturbTheSeverityLevel) {
  nsclient_logger logger;
  logger.set_log_level("warning");
  for (const std::string option : {"console", "no-console", "oneline", "no-std-err"}) {
    SCOPED_TRACE("option=" + option);
    EXPECT_TRUE(nsclient_logger::is_console_option(option));
    EXPECT_NO_THROW(logger.set_log_level(option));
    EXPECT_EQ(logger.get_log_level(), "warning");
  }
}

TEST(NsclientLogger, LinesBelowTheLevelReachNoSubscriber) {
  nsclient_logger logger;
  auto sub = std::make_shared<CapturingSubscriber>();
  logger.add_subscriber(sub);
  logger.set_log_level("error");
  logger.debug("m", __FILE__, __LINE__, "debug-msg");
  logger.warning("m", __FILE__, __LINE__, "warning-msg");
  logger.error("m", __FILE__, __LINE__, "error-msg");
  EXPECT_EQ(sub->wait_for(1), (std::vector<std::string>{"error-msg"}));
}

// ===== fan-out ===============================================================

TEST(NsclientLogger, EverySubscriberGetsEveryLineInOrder) {
  nsclient_logger logger;
  auto a = std::make_shared<CapturingSubscriber>();
  auto b = std::make_shared<CapturingSubscriber>();
  logger.add_subscriber(a);
  logger.add_subscriber(b);
  logger.do_log(line("one"));
  logger.do_log(line("two"));
  EXPECT_EQ(a->wait_for(2), (std::vector<std::string>{"one", "two"}));
  EXPECT_EQ(b->wait_for(2), (std::vector<std::string>{"one", "two"}));
}

TEST(NsclientLogger, ClearSubscribersStopsTheFanOut) {
  nsclient_logger logger;
  auto sub = std::make_shared<CapturingSubscriber>();
  logger.add_subscriber(sub);
  logger.do_log(line("first"));
  ASSERT_EQ(sub->wait_for(1).size(), 1u);
  EXPECT_TRUE(logger.clear_subscribers().empty());
  logger.do_log(line("ignored"));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(sub->snapshot().size(), 1u);
}

// A handler that logs once per line it receives must not feed itself, nor
// another handler that does the same.
TEST(NsclientLogger, ALineLoggedFromAHandlerReachesNoSubscriber) {
  nsclient_logger logger;
  auto a = std::make_shared<ReentrantSubscriber>(&logger);
  auto b = std::make_shared<ReentrantSubscriber>(&logger);
  logger.add_subscriber(a);
  logger.add_subscriber(b);
  logger.do_log(line("outer"));
  ASSERT_EQ(a->wait_for(1).size(), 1u);
  ASSERT_EQ(b->wait_for(1).size(), 1u);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(a->snapshot(), (std::vector<std::string>{"outer"}));
  EXPECT_EQ(b->snapshot(), (std::vector<std::string>{"outer"}));
}

// A stuck handler must not hold up the threads that log.
TEST(NsclientLogger, AStuckHandlerDoesNotBlockLogging) {
  nsclient_logger logger;
  logger.set_delivery_wait(std::chrono::milliseconds(50));
  logger.set_join_wait(std::chrono::milliseconds(50));
  auto stuck = std::make_shared<BlockingSubscriber>();
  logger.add_subscriber(stuck);
  logger.do_log(line("enter"));
  stuck->wait_until_entered();
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100; ++i) logger.do_log(line("more"));
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
  stuck->release();
}

TEST(NsclientLogger, AThrowingHandlerDoesNotStopTheOthers) {
  struct Throwing : logging_subscriber {
    void on_log_message(const std::string&) override { throw std::runtime_error("boom"); }
  };
  nsclient_logger logger;
  auto good = std::make_shared<CapturingSubscriber>();
  logger.add_subscriber(std::make_shared<Throwing>());
  logger.add_subscriber(good);
  logger.do_log(line("one"));
  logger.do_log(line("two"));
  EXPECT_EQ(good->wait_for(2), (std::vector<std::string>{"one", "two"}));
}

// ===== removal ===============================================================

TEST(NsclientLogger, RemoveReportsWhetherTheSubscriberWasOnTheList) {
  nsclient_logger logger;
  auto sub = std::make_shared<CapturingSubscriber>();
  EXPECT_FALSE(logger.remove_subscriber(sub).removed);
  logger.add_subscriber(sub);
  const auto result = logger.remove_subscriber(sub);
  EXPECT_TRUE(result.removed);
  EXPECT_FALSE(result.delivering);
  EXPECT_FALSE(logger.remove_subscriber(sub).removed);
}

TEST(NsclientLogger, RemoveWaitsForTheLineInsideTheSubscriber) {
  nsclient_logger logger;
  auto sub = std::make_shared<BlockingSubscriber>();
  logger.add_subscriber(sub);
  logger.do_log(line("x"));
  sub->wait_until_entered();
  std::thread releaser([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sub->release();
  });
  const auto result = logger.remove_subscriber(sub);
  EXPECT_TRUE(result.removed);
  EXPECT_FALSE(result.delivering);
  EXPECT_TRUE(sub->finished);
  releaser.join();
}

TEST(NsclientLogger, RemoveDoesNotWaitForADeliveryInsideAnotherSubscriber) {
  nsclient_logger logger;
  logger.set_delivery_wait(std::chrono::seconds(5));
  auto stuck = std::make_shared<BlockingSubscriber>();
  auto other = std::make_shared<CapturingSubscriber>();
  logger.add_subscriber(stuck);
  logger.add_subscriber(other);
  logger.do_log(line("x"));
  stuck->wait_until_entered();
  const auto start = std::chrono::steady_clock::now();
  const auto result = logger.remove_subscriber(other);
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
  EXPECT_TRUE(result.removed);
  EXPECT_FALSE(result.delivering);
  stuck->release();
}

TEST(NsclientLogger, RemoveReportsADeliveryThatOutlivesTheWait) {
  nsclient_logger logger;
  logger.set_delivery_wait(std::chrono::milliseconds(50));
  auto sub = std::make_shared<BlockingSubscriber>();
  logger.add_subscriber(sub);
  logger.do_log(line("x"));
  sub->wait_until_entered();
  const auto result = logger.remove_subscriber(sub);
  EXPECT_TRUE(result.removed);
  EXPECT_TRUE(result.delivering);
  sub->release();
}

TEST(NsclientLogger, CloseKeepsTheSubscriberInPlaceAndReopenResumesIt) {
  nsclient_logger logger;
  auto a = std::make_shared<CapturingSubscriber>();
  auto b = std::make_shared<CapturingSubscriber>();
  logger.add_subscriber(a);
  logger.add_subscriber(b);
  const auto closed = logger.close_subscriber(a);
  EXPECT_TRUE(closed.removed);
  EXPECT_FALSE(closed.delivering);
  logger.do_log(line("while-closed"));
  ASSERT_EQ(b->wait_for(1).size(), 1u);
  logger.reopen_subscriber(a);
  logger.do_log(line("after"));
  ASSERT_EQ(b->wait_for(2).size(), 2u);
  EXPECT_EQ(a->wait_for(1), (std::vector<std::string>{"after"}));
}

// A second add of a subscriber already on the list reopens it rather than
// adding another entry that would get every line twice.
TEST(NsclientLogger, AddingASubscriberAgainReopensIt) {
  nsclient_logger logger;
  auto a = std::make_shared<CapturingSubscriber>();
  logger.add_subscriber(a);
  logger.close_subscriber(a);
  logger.add_subscriber(a);
  logger.add_subscriber(a);
  logger.do_log(line("once"));
  EXPECT_EQ(a->wait_for(1), (std::vector<std::string>{"once"}));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(a->snapshot().size(), 1u);
}

// A close refused because a line is inside leaves the subscriber closed in
// place, so a retry waits for that same line again instead of finding it
// clear - and a drop is refused until it has left.
TEST(NsclientLogger, ARetriedCloseWaitsForTheSameLineAgain) {
  nsclient_logger logger;
  logger.set_delivery_wait(std::chrono::milliseconds(50));
  auto stuck = std::make_shared<BlockingSubscriber>();
  logger.add_subscriber(stuck);
  logger.do_log(line("x"));
  stuck->wait_until_entered();
  EXPECT_TRUE(logger.close_subscriber(stuck).delivering);
  EXPECT_TRUE(logger.close_subscriber(stuck).delivering);
  EXPECT_FALSE(logger.drop_subscriber(stuck));
  stuck->release();
  logger.set_delivery_wait(std::chrono::seconds(5));
  const auto result = logger.close_subscriber(stuck);
  EXPECT_TRUE(result.removed);
  EXPECT_FALSE(result.delivering);
  EXPECT_TRUE(logger.drop_subscriber(stuck));
  EXPECT_FALSE(logger.close_subscriber(stuck).removed);
}

TEST(NsclientLogger, ClearNamesTheSubscriberStillDelivering) {
  nsclient_logger logger;
  logger.set_delivery_wait(std::chrono::milliseconds(50));
  auto stuck = std::make_shared<BlockingSubscriber>();
  logger.add_subscriber(stuck);
  logger.add_subscriber(std::make_shared<CapturingSubscriber>());
  logger.do_log(line("x"));
  stuck->wait_until_entered();
  const auto still = logger.clear_subscribers();
  ASSERT_EQ(still.size(), 1u);
  EXPECT_EQ(still[0], stuck);
  stuck->release();
}

// A removal that timed out is still caught by the clear at shutdown.
TEST(NsclientLogger, AStuckRemovalIsWaitedForAgainByClear) {
  nsclient_logger logger;
  logger.set_delivery_wait(std::chrono::milliseconds(50));
  auto stuck = std::make_shared<BlockingSubscriber>();
  logger.add_subscriber(stuck);
  logger.do_log(line("x"));
  stuck->wait_until_entered();
  ASSERT_TRUE(logger.remove_subscriber(stuck).delivering);
  const auto still = logger.clear_subscribers();
  ASSERT_EQ(still.size(), 1u);
  EXPECT_EQ(still[0], stuck);
  stuck->release();
  EXPECT_TRUE(logger.clear_subscribers().empty());
}

TEST(NsclientLogger, ASubscriberMayRemoveItselfFromItsHandler) {
  nsclient_logger logger;
  auto sub = std::make_shared<SelfRemovingSubscriber>(&logger);
  auto after = std::make_shared<CapturingSubscriber>();
  logger.add_subscriber(sub);
  logger.add_subscriber(after);
  logger.do_log(line("one"));
  logger.do_log(line("two"));
  ASSERT_EQ(after->wait_for(2).size(), 2u);
  EXPECT_TRUE(sub->removed);
  EXPECT_EQ(sub->calls, 1);
}

// The worker never holds the last reference: the remover does.
TEST(NsclientLogger, TheRemoverHoldsTheLastReference) {
  nsclient_logger logger;
  auto sub = std::make_shared<BlockingSubscriber>();
  logger.add_subscriber(sub);
  logger.do_log(line("x"));
  sub->wait_until_entered();
  std::thread releaser([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sub->release();
  });
  ASSERT_FALSE(logger.remove_subscriber(sub).delivering);
  releaser.join();
  EXPECT_EQ(sub.use_count(), 1);
}

// A worker stuck in a handler at shutdown is left behind, and the logger can
// still be destroyed under it.
TEST(NsclientLogger, ShutdownLeavesAStuckWorkerBehind) {
  auto stuck = std::make_shared<BlockingSubscriber>();
  {
    nsclient_logger logger;
    logger.set_delivery_wait(std::chrono::milliseconds(50));
    logger.set_join_wait(std::chrono::milliseconds(50));
    logger.add_subscriber(stuck);
    logger.do_log(line("x"));
    stuck->wait_until_entered();
    EXPECT_FALSE(logger.shutdown());
  }
  stuck->release();
  for (int i = 0; i < 100 && !stuck->finished; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(stuck->finished);
}
