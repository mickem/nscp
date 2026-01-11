#include <gtest/gtest.h>

#include <boost/asio/ip/address.hpp>
#include <socket/allowed_hosts.hpp>

using namespace socket_helpers;
using namespace boost::asio::ip;

TEST(AllowedHostsTest, EmptyConfigAllowsAll) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;
  EXPECT_TRUE(manager.is_allowed(address::from_string("127.0.0.1"), errors));
  EXPECT_TRUE(manager.is_allowed(address::from_string("::1"), errors));
}

TEST(AllowedHostsTest, AddressSpanShouldBeAllowedV4) {
  const auto allowed = address_v4::from_string("192.168.1.0").to_bytes();
  const auto mask = address_v4::from_string("255.255.255.0").to_bytes();
  const auto remote_match = address_v4::from_string("192.168.1.100").to_bytes();
  const auto remote_mismatch = address_v4::from_string("192.168.2.100").to_bytes();

  EXPECT_TRUE(allowed_hosts_manager::match_host(allowed, mask, remote_match));
  EXPECT_FALSE(allowed_hosts_manager::match_host(allowed, mask, remote_mismatch));
}

TEST(AllowedHostsTest, AddressSpanShouldBeAllowedV6) {
  const auto allowed = address_v6::from_string("2001:db8::").to_bytes();
  const auto mask = address_v6::from_string("ffff:ffff::").to_bytes();
  const auto remote_match = address_v6::from_string("2001:db8::1").to_bytes();
  const auto remote_mismatch = address_v6::from_string("2001:db9::1").to_bytes();

  EXPECT_TRUE(allowed_hosts_manager::match_host(allowed, mask, remote_match));
  EXPECT_FALSE(allowed_hosts_manager::match_host(allowed, mask, remote_mismatch));
}

TEST(AllowedHostsTest, SingleAddressIsAllowedV4) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;
  const auto addr = address_v4::from_string("10.0.0.1").to_bytes();
  const auto mask = address_v4::from_string("255.255.255.255").to_bytes();
  manager.entries_v4.emplace_back("test", addr, mask);

  EXPECT_TRUE(manager.is_allowed(address::from_string("10.0.0.1"), errors));
  EXPECT_FALSE(manager.is_allowed(address::from_string("10.0.0.2"), errors));
}

TEST(AllowedHostsTest, SingleAddressIsAllowedV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  auto addr = address_v6::from_string("::1").to_bytes();
  auto mask = address_v6::from_string("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").to_bytes();
  manager.entries_v6.emplace_back("test", addr, mask);

  EXPECT_TRUE(manager.is_allowed(address::from_string("::1"), errors));
  EXPECT_FALSE(manager.is_allowed(address::from_string("::2"), errors));
}

TEST(AllowedHostsTest, IsAllowedV4MappedV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  const auto addr = address_v4::from_string("192.168.0.1").to_bytes();
  const auto mask = address_v4::from_string("255.255.255.255").to_bytes();
  manager.entries_v4.emplace_back("test", addr, mask);

  EXPECT_TRUE(manager.is_allowed(address::from_string("::ffff:192.168.0.1"), errors));
  EXPECT_FALSE(manager.is_allowed(address::from_string("::ffff:192.168.0.2"), errors));
}

TEST(AllowedHostsTest, MixedV4AndV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  manager.entries_v4.emplace_back("local_v4", address_v4::from_string("127.0.0.1").to_bytes(), address_v4::from_string("255.255.255.255").to_bytes());

  manager.entries_v6.emplace_back("local_v6", address_v6::from_string("::1").to_bytes(),
                                  address_v6::from_string("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").to_bytes());

  EXPECT_TRUE(manager.is_allowed(address::from_string("127.0.0.1"), errors));
  EXPECT_TRUE(manager.is_allowed(address::from_string("::1"), errors));

  EXPECT_FALSE(manager.is_allowed(address::from_string("127.0.0.2"), errors));
  EXPECT_FALSE(manager.is_allowed(address::from_string("::2"), errors));
}