// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include "check_radius_resolver.hpp"

#include <gtest/gtest.h>

using namespace check_net::radius;
TEST(RadiusResolver, NumericAddressesDoNotNeedAChildProcess) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  const auto v4 = resolve_host("127.0.0.1", net::address_family::ipv4, deadline);
  EXPECT_TRUE(v4.error.empty());
  EXPECT_EQ(v4.address.to_string(), "127.0.0.1");
  const auto v6 = resolve_host("::1", net::address_family::ipv6, deadline);
  EXPECT_TRUE(v6.error.empty());
  EXPECT_EQ(v6.address.to_string(), "::1");
  EXPECT_EQ(resolve_host("::1", net::address_family::ipv4, deadline).error, "resolve_failed");
  EXPECT_EQ(resolve_host("127.0.0.1", net::address_family::ipv6, deadline).error, "resolve_failed");
}

TEST(RadiusResolver, TerminatesAndReapsAStalledChildAtTheDeadline) {
  // Use this test executable as a real, indefinitely running child. Selecting
  // just the numeric test avoids recursion and needs no external DNS or tools.
  const auto start = std::chrono::steady_clock::now();
  const auto result =
      resolve_process(boost::dll::program_location(), {"--gtest_filter=RadiusResolver.NumericAddressesDoNotNeedAChildProcess", "--gtest_repeat=-1"},
                      start + std::chrono::milliseconds(100));
  EXPECT_EQ(result.error, "timeout");
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(2));
}

TEST(RadiusResolver, RejectsAnExpiredDeadlineBeforeStartingAChild) {
  EXPECT_EQ(resolve_process("nonexistent-resolver", {}, std::chrono::steady_clock::now() - std::chrono::seconds(1)).error, "timeout");
}
