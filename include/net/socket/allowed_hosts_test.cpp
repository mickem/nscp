// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/asio/ip/address.hpp>
#include <net/socket/allowed_hosts.hpp>

using namespace socket_helpers;
using namespace boost::asio::ip;

TEST(AllowedHostsTest, EmptyConfigRejectsAll) {
  // Fail-closed: the previous behaviour returned true for every IP when the
  // list was empty, which meant a fat-finger that cleared the setting
  // silently dropped the agent into "any IP wins". Per-module defaults
  // (typically `127.0.0.1`) mean an empty list represents an operator who
  // explicitly cleared the setting - and they can re-state intent with
  // `allowed hosts = 0.0.0.0/0,::/0`.
  allowed_hosts_manager manager;
  std::list<std::string> errors;
  EXPECT_FALSE(manager.is_allowed(make_address("127.0.0.1"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("::1"), errors));
  EXPECT_FALSE(errors.empty()) << "rejection should produce a clear error message";
}

TEST(AllowedHostsTest, ExplicitAllAllowsAll) {
  // The escape hatch for operators who really do want every source to
  // connect: an explicit 0.0.0.0/0,::/0 source.
  allowed_hosts_manager manager;
  manager.set_source("0.0.0.0/0,::/0");
  std::list<std::string> errors;
  manager.refresh(errors);
  EXPECT_TRUE(manager.is_allowed(make_address("127.0.0.1"), errors));
  EXPECT_TRUE(manager.is_allowed(make_address("8.8.8.8"), errors));
  EXPECT_TRUE(manager.is_allowed(make_address("::1"), errors));
}

TEST(AllowedHostsTest, AddressSpanShouldBeAllowedV4) {
  const auto allowed = make_address_v4("192.168.1.0").to_bytes();
  const auto mask = make_address_v4("255.255.255.0").to_bytes();
  const auto remote_match = make_address_v4("192.168.1.100").to_bytes();
  const auto remote_mismatch = make_address_v4("192.168.2.100").to_bytes();

  EXPECT_TRUE(allowed_hosts_manager::match_host(allowed, mask, remote_match));
  EXPECT_FALSE(allowed_hosts_manager::match_host(allowed, mask, remote_mismatch));
}

TEST(AllowedHostsTest, AddressSpanShouldBeAllowedV6) {
  const auto allowed = make_address_v6("2001:db8::").to_bytes();
  const auto mask = make_address_v6("ffff:ffff::").to_bytes();
  const auto remote_match = make_address_v6("2001:db8::1").to_bytes();
  const auto remote_mismatch = make_address_v6("2001:db9::1").to_bytes();

  EXPECT_TRUE(allowed_hosts_manager::match_host(allowed, mask, remote_match));
  EXPECT_FALSE(allowed_hosts_manager::match_host(allowed, mask, remote_mismatch));
}

TEST(AllowedHostsTest, SingleAddressIsAllowedV4) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;
  const auto addr = make_address_v4("10.0.0.1").to_bytes();
  const auto mask = make_address_v4("255.255.255.255").to_bytes();
  manager.entries_v4.emplace_back("test", addr, mask);

  EXPECT_TRUE(manager.is_allowed(make_address("10.0.0.1"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("10.0.0.2"), errors));
}

TEST(AllowedHostsTest, SingleAddressIsAllowedV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  auto addr = make_address_v6("::1").to_bytes();
  auto mask = make_address_v6("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").to_bytes();
  manager.entries_v6.emplace_back("test", addr, mask);

  EXPECT_TRUE(manager.is_allowed(make_address("::1"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("::2"), errors));
}

TEST(AllowedHostsTest, IsAllowedV4MappedV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  const auto addr = make_address_v4("192.168.0.1").to_bytes();
  const auto mask = make_address_v4("255.255.255.255").to_bytes();
  manager.entries_v4.emplace_back("test", addr, mask);

  EXPECT_TRUE(manager.is_allowed(make_address("::ffff:192.168.0.1"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("::ffff:192.168.0.2"), errors));
}

TEST(AllowedHostsTest, MixedV4AndV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  manager.entries_v4.emplace_back("local_v4", make_address_v4("127.0.0.1").to_bytes(), make_address_v4("255.255.255.255").to_bytes());

  manager.entries_v6.emplace_back("local_v6", make_address_v6("::1").to_bytes(), make_address_v6("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").to_bytes());

  EXPECT_TRUE(manager.is_allowed(make_address("127.0.0.1"), errors));
  EXPECT_TRUE(manager.is_allowed(make_address("::1"), errors));

  EXPECT_FALSE(manager.is_allowed(make_address("127.0.0.2"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("::2"), errors));
}