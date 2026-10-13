// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <net/connection_log_throttle.hpp>

using net::connection_log_throttle;

namespace {
const connection_log_throttle::clock::time_point t0 = connection_log_throttle::clock::now();
connection_log_throttle::clock::time_point at(const int seconds) { return t0 + std::chrono::seconds(seconds); }
}  // namespace

TEST(ConnectionLogThrottle, LogsTheFirstRefusalThenOncePerWindowWithACount) {
  connection_log_throttle throttle(std::chrono::seconds(60));
  std::size_t suppressed = 99;
  EXPECT_TRUE(throttle.should_log_at("192.0.2.1", suppressed, at(0)));
  EXPECT_EQ(suppressed, 0u);
  for (int i = 1; i <= 5; i++) EXPECT_FALSE(throttle.should_log_at("192.0.2.1", suppressed, at(i)));
  EXPECT_TRUE(throttle.should_log_at("192.0.2.1", suppressed, at(60)));
  EXPECT_EQ(suppressed, 5u);
  EXPECT_FALSE(throttle.should_log_at("192.0.2.1", suppressed, at(61)));
}

TEST(ConnectionLogThrottle, AddressesAreThrottledSeparately) {
  connection_log_throttle throttle(std::chrono::seconds(60));
  std::size_t suppressed = 0;
  EXPECT_TRUE(throttle.should_log_at("192.0.2.1", suppressed, at(0)));
  EXPECT_TRUE(throttle.should_log_at("192.0.2.2", suppressed, at(1)));
  EXPECT_FALSE(throttle.should_log_at("192.0.2.1", suppressed, at(2)));
}

TEST(ConnectionLogThrottle, ATableFullOfAddressesSharesOneEntry) {
  // Many addresses (an IPv6 /64 is plenty) cannot grow the table, nor get a
  // line each.
  connection_log_throttle throttle(std::chrono::seconds(60), 4);
  std::size_t suppressed = 0;
  for (int i = 0; i < 4; i++) EXPECT_TRUE(throttle.should_log_at("2001:db8::" + std::to_string(i), suppressed, at(0)));
  // Still refused, so their entries have something pending and stay.
  for (int i = 0; i < 4; i++) EXPECT_FALSE(throttle.should_log_at("2001:db8::" + std::to_string(i), suppressed, at(1)));
  EXPECT_TRUE(throttle.should_log_at("2001:db8::100", suppressed, at(1)));
  int logged = 0;
  for (int i = 101; i < 200; i++) {
    if (throttle.should_log_at("2001:db8::" + std::to_string(i), suppressed, at(2))) logged++;
  }
  EXPECT_EQ(logged, 0);
  EXPECT_LE(throttle.tracked(), 5u);
  EXPECT_TRUE(throttle.should_log_at("2001:db8::300", suppressed, at(61)));
  EXPECT_EQ(suppressed, 99u);
}

TEST(ConnectionLogThrottle, ExpiredQuietAddressesMakeRoom) {
  connection_log_throttle throttle(std::chrono::seconds(60), 2);
  std::size_t suppressed = 0;
  EXPECT_TRUE(throttle.should_log_at("192.0.2.1", suppressed, at(0)));
  EXPECT_TRUE(throttle.should_log_at("192.0.2.2", suppressed, at(0)));
  // Both windows have passed with nothing pending, so a new address gets an
  // entry of its own rather than the overflow one.
  EXPECT_TRUE(throttle.should_log_at("192.0.2.3", suppressed, at(70)));
  EXPECT_FALSE(throttle.should_log_at("192.0.2.3", suppressed, at(71)));
  EXPECT_TRUE(throttle.should_log_at("192.0.2.4", suppressed, at(71)));
}
