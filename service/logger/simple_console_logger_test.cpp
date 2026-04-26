/*
 * Unit tests for nsclient::logging::impl::simple_console_logger.
 *
 * Note: simple_console_logger's constructor calls
 *   std::cout.rdbuf()->pubsetbuf(buf_.data(), buf_.size())
 * which can affect process-global cout state. To minimise interference with
 * gtest's own cout usage (especially around its summary printing) we only
 * construct one instance per test, scope it tightly, and avoid asserting on
 * captured stdout/stderr content.
 */

#include "simple_console_logger.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <mutex>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>
#include <vector>

using nsclient::logging::log_message_factory;
using nsclient::logging::logging_subscriber;
using nsclient::logging::impl::simple_console_logger;

namespace {

class CapturingSubscriber : public logging_subscriber {
 public:
  void on_log_message(const std::string& payload) override {
    std::lock_guard<std::mutex> g(mu);
    payloads.push_back(payload);
  }
  std::vector<std::string> payloads;
  std::mutex mu;
};

}  // namespace

TEST(SimpleConsoleLogger, ConstructionDoesNotCrash) {
  CapturingSubscriber sub;
  simple_console_logger logger(&sub);
  EXPECT_FALSE(logger.is_started());
  EXPECT_FALSE(logger.is_console());
  EXPECT_FALSE(logger.is_oneline());
}

TEST(SimpleConsoleLogger, DoLogAlwaysNotifiesSubscriber) {
  CapturingSubscriber sub;
  simple_console_logger logger(&sub);
  const std::string payload = log_message_factory::create_info("test", __FILE__, __LINE__, "hello");
  logger.do_log(payload);
  ASSERT_EQ(sub.payloads.size(), 1u);
  EXPECT_EQ(sub.payloads[0], payload);
}

TEST(SimpleConsoleLogger, DoLogForwardsRawDataIncludingMalformed) {
  CapturingSubscriber sub;
  simple_console_logger logger(&sub);
  logger.do_log("not a protobuf");
  logger.do_log("");
  EXPECT_EQ(sub.payloads.size(), 2u);
  EXPECT_EQ(sub.payloads[0], "not a protobuf");
  EXPECT_EQ(sub.payloads[1], "");
}

TEST(SimpleConsoleLogger, MultipleDoLogCallsRetainOrder) {
  CapturingSubscriber sub;
  simple_console_logger logger(&sub);
  for (int i = 0; i < 10; ++i) {
    logger.do_log("entry-" + std::to_string(i));
  }
  ASSERT_EQ(sub.payloads.size(), 10u);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(sub.payloads[static_cast<size_t>(i)], "entry-" + std::to_string(i));
  }
}

TEST(SimpleConsoleLogger, BaseClassFlagsToggleViaSetConfig) {
  CapturingSubscriber sub;
  simple_console_logger logger(&sub);
  EXPECT_FALSE(logger.is_oneline());
  logger.set_config("oneline");
  EXPECT_TRUE(logger.is_oneline());

  EXPECT_FALSE(logger.is_no_std_err());
  logger.set_config("no-std-err");
  EXPECT_TRUE(logger.is_no_std_err());

  EXPECT_FALSE(logger.is_console());
  logger.set_config("console");
  EXPECT_TRUE(logger.is_console());
}

TEST(SimpleConsoleLogger, SetConfigDoesNotAffectStartedFlag) {
  CapturingSubscriber sub;
  simple_console_logger logger(&sub);
  EXPECT_FALSE(logger.is_started());
  logger.set_config("oneline");
  EXPECT_FALSE(logger.is_started());
}
