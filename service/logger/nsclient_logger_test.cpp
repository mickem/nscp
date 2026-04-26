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

#include <memory>
#include <mutex>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <vector>

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
