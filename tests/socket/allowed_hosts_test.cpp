#include <gtest/gtest.h>

#include <boost/asio/ip/address.hpp>
#include <socket/allowed_hosts.hpp>

using namespace socket_helpers;

TEST(AllowedHostsTest, EmptyConfigAllowsAll) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;
  // When no hosts are defined, everything is allowed (fail-open for empty config)
  EXPECT_TRUE(manager.is_allowed(boost::asio::ip::address::from_string("127.0.0.1"), errors));
  EXPECT_TRUE(manager.is_allowed(boost::asio::ip::address::from_string("::1"), errors));
}

TEST(AllowedHostsTest, MatchHostLogicV4) {
  using bytes_type = boost::asio::ip::address_v4::bytes_type;
  // 192.168.1.0
  auto allowed = boost::asio::ip::address_v4::from_string("192.168.1.0").to_bytes();
  // 255.255.255.0
  auto mask = boost::asio::ip::address_v4::from_string("255.255.255.0").to_bytes();

  // 192.168.1.100 matches
  auto remote_match = boost::asio::ip::address_v4::from_string("192.168.1.100").to_bytes();
  // 192.168.2.100 does not match
  auto remote_mismatch = boost::asio::ip::address_v4::from_string("192.168.2.100").to_bytes();

  EXPECT_TRUE(allowed_hosts_manager::match_host(allowed, mask, remote_match));
  EXPECT_FALSE(allowed_hosts_manager::match_host(allowed, mask, remote_mismatch));
}

TEST(AllowedHostsTest, MatchHostLogicV6) {
  using bytes_type = boost::asio::ip::address_v6::bytes_type;
  // 2001:db8::
  auto allowed = boost::asio::ip::address_v6::from_string("2001:db8::").to_bytes();
  // /32 mask (ffff:ffff::)
  auto mask = boost::asio::ip::address_v6::from_string("ffff:ffff::").to_bytes();

  auto remote_match = boost::asio::ip::address_v6::from_string("2001:db8::1").to_bytes();
  auto remote_mismatch = boost::asio::ip::address_v6::from_string("2001:db9::1").to_bytes();

  EXPECT_TRUE(allowed_hosts_manager::match_host(allowed, mask, remote_match));
  EXPECT_FALSE(allowed_hosts_manager::match_host(allowed, mask, remote_mismatch));
}

TEST(AllowedHostsTest, IsAllowedV4) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  // Inject a rule: allow 10.0.0.1
  auto addr = boost::asio::ip::address_v4::from_string("10.0.0.1").to_bytes();
  auto mask = boost::asio::ip::address_v4::from_string("255.255.255.255").to_bytes();
  manager.entries_v4.push_back(allowed_hosts_manager::host_record_v4("test", addr, mask));

  // 10.0.0.1 should be allowed
  EXPECT_TRUE(manager.is_allowed(boost::asio::ip::address::from_string("10.0.0.1"), errors));
  // 10.0.0.2 should be denied
  EXPECT_FALSE(manager.is_allowed(boost::asio::ip::address::from_string("10.0.0.2"), errors));
}

TEST(AllowedHostsTest, IsAllowedV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  // Inject a rule: allow ::1
  auto addr = boost::asio::ip::address_v6::from_string("::1").to_bytes();
  auto mask = boost::asio::ip::address_v6::from_string("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").to_bytes();
  manager.entries_v6.push_back(allowed_hosts_manager::host_record_v6("test", addr, mask));

  EXPECT_TRUE(manager.is_allowed(boost::asio::ip::address::from_string("::1"), errors));
  EXPECT_FALSE(manager.is_allowed(boost::asio::ip::address::from_string("::2"), errors));
}

TEST(AllowedHostsTest, IsAllowedV4MappedV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  // Inject a rule: allow 192.168.0.1
  auto addr = boost::asio::ip::address_v4::from_string("192.168.0.1").to_bytes();
  auto mask = boost::asio::ip::address_v4::from_string("255.255.255.255").to_bytes();
  manager.entries_v4.push_back(allowed_hosts_manager::host_record_v4("test", addr, mask));

  // Check ::ffff:192.168.0.1
  EXPECT_TRUE(manager.is_allowed(boost::asio::ip::address::from_string("::ffff:192.168.0.1"), errors));
  // Check ::ffff:192.168.0.2
  EXPECT_FALSE(manager.is_allowed(boost::asio::ip::address::from_string("::ffff:192.168.0.2"), errors));
}

TEST(AllowedHostsTest, MixedV4AndV6) {
  allowed_hosts_manager manager;
  std::list<std::string> errors;

  // Allow 127.0.0.1
  manager.entries_v4.push_back(allowed_hosts_manager::host_record_v4("local_v4", boost::asio::ip::address_v4::from_string("127.0.0.1").to_bytes(),
                                                                     boost::asio::ip::address_v4::from_string("255.255.255.255").to_bytes()));

  // Allow ::1
  manager.entries_v6.push_back(
      allowed_hosts_manager::host_record_v6("local_v6", boost::asio::ip::address_v6::from_string("::1").to_bytes(),
                                            boost::asio::ip::address_v6::from_string("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff").to_bytes()));

  EXPECT_TRUE(manager.is_allowed(boost::asio::ip::address::from_string("127.0.0.1"), errors));
  EXPECT_TRUE(manager.is_allowed(boost::asio::ip::address::from_string("::1"), errors));

  EXPECT_FALSE(manager.is_allowed(boost::asio::ip::address::from_string("127.0.0.2"), errors));
  EXPECT_FALSE(manager.is_allowed(boost::asio::ip::address::from_string("::2"), errors));
}