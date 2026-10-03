// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/asio/ip/address.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <mutex>
#include <net/socket/allowed_hosts.hpp>
#include <thread>

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
// ---------------------------------------------------------------------------
// The `*` range syntax. The setting's own help text and the permissions docs
// have advertised it for years; make_address threw on it instead, outside the
// try that wraps the DNS branch, so the exception escaped refresh() and (with
// `cache allowed hosts = true`) loadModuleEx - the listener never started.
// ---------------------------------------------------------------------------

TEST(AllowedHostsTest, TrailingWildcardIsATwentyFourBitRange) {
  allowed_hosts_manager manager;
  manager.set_source("192.168.1.*");
  std::list<std::string> errors;
  manager.refresh(errors);

  EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
  EXPECT_TRUE(manager.is_allowed(make_address("192.168.1.1"), errors));
  EXPECT_TRUE(manager.is_allowed(make_address("192.168.1.254"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("192.168.2.1"), errors));
}

TEST(AllowedHostsTest, ShorterWildcardsWidenTheRange) {
  allowed_hosts_manager manager;
  manager.set_source("10.*");
  std::list<std::string> errors;
  manager.refresh(errors);

  EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
  EXPECT_TRUE(manager.is_allowed(make_address("10.0.0.1"), errors));
  EXPECT_TRUE(manager.is_allowed(make_address("10.255.255.255"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("11.0.0.1"), errors));
}

TEST(AllowedHostsTest, ABareWildcardIsEveryAddress) {
  allowed_hosts_manager manager;
  manager.set_source("*");
  std::list<std::string> errors;
  manager.refresh(errors);

  EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
  EXPECT_TRUE(manager.is_allowed(make_address("8.8.8.8"), errors));
}

TEST(AllowedHostsTest, WildcardsMayRepeatPerOctet) {
  allowed_hosts_manager manager;
  manager.set_source("192.168.*.*");
  std::list<std::string> errors;
  manager.refresh(errors);

  EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
  EXPECT_TRUE(manager.is_allowed(make_address("192.168.7.9"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("192.169.7.9"), errors));
}

TEST(AllowedHostsTest, AWildcardInTheMiddleIsAnError) {
  // Not a prefix, so there is no range it could mean.
  allowed_hosts_manager manager;
  manager.set_source("192.*.1.1");
  std::list<std::string> errors;
  manager.refresh(errors);

  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(manager.is_allowed(make_address("192.168.1.1"), errors));
}

TEST(AllowedHostsTest, AWildcardWithAMaskIsAnError) {
  allowed_hosts_manager manager;
  manager.set_source("192.168.1.*/24");
  std::list<std::string> errors;
  manager.refresh(errors);

  EXPECT_FALSE(errors.empty());
}

TEST(AllowedHostsTest, AnUnparseableNumericAddressIsReportedNotThrown) {
  // The throw used to escape refresh(); with `cache allowed hosts = true` the
  // plugin manager then dropped the whole module.
  allowed_hosts_manager manager;
  manager.set_source("192.168.1.999,10.0.0.1");
  std::list<std::string> errors;

  ASSERT_NO_THROW(manager.refresh(errors));
  EXPECT_FALSE(errors.empty());
  // The good entry in the same list still works.
  EXPECT_TRUE(manager.is_allowed(make_address("10.0.0.1"), errors));
}

TEST(AllowedHostsTest, AFailedLookupKeepsWhatTheNameResolvedToBefore) {
  // A name that resolved once and then fails a lookup (a DNS hiccup) keeps
  // its addresses: the list used to be emptied first and rebuilt, so the host
  // was refused until the next successful refresh.
  allowed_hosts_manager manager;
  manager.set_source("no-such-host.invalid");
  manager.entries_v4.emplace_back("no-such-host.invalid", make_address("192.0.2.7").to_v4().to_bytes(), allowed_hosts_manager::addr_v4{{255, 255, 255, 255}});
  std::list<std::string> errors;
  manager.refresh(errors);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(errors.front().find("keeping the addresses it resolved to before"), std::string::npos) << errors.front();
  std::list<std::string> check_errors;
  EXPECT_TRUE(manager.is_allowed(make_address("192.0.2.7"), check_errors));
}

TEST(AllowedHostsTest, ARemovedNameDoesNotLinger) {
  // Only a failed lookup keeps old entries; a name taken out of the list is
  // gone at the next refresh.
  allowed_hosts_manager manager;
  manager.set_source("127.0.0.1");
  manager.entries_v4.emplace_back("no-such-host.invalid", make_address("192.0.2.7").to_v4().to_bytes(), allowed_hosts_manager::addr_v4{{255, 255, 255, 255}});
  std::list<std::string> errors;
  manager.refresh(errors);
  EXPECT_FALSE(manager.is_allowed(make_address("192.0.2.7"), errors));
  EXPECT_TRUE(manager.is_allowed(make_address("127.0.0.1"), errors));
}

TEST(AllowedHostsTest, ANameListedTwiceIsLookedUpOnce) {
  allowed_hosts_manager manager;
  manager.set_source("no-such-host.invalid, 127.0.0.1,no-such-host.invalid");
  EXPECT_EQ(manager.sources.size(), 2u);
}

TEST(AllowedHostsTest, AFailingNameListedTwiceDoesNotGrowTheList) {
  // Each failed lookup used to copy the name's old entries once per listing,
  // doubling the list on every refresh. The sources are pushed directly here,
  // past set_source()'s own de-duplication.
  allowed_hosts_manager manager;
  manager.sources = {"no-such-host.invalid", "no-such-host.invalid"};
  manager.entries_v4.emplace_back("no-such-host.invalid", make_address("192.0.2.7").to_v4().to_bytes(), allowed_hosts_manager::addr_v4{{255, 255, 255, 255}});
  for (int i = 0; i < 5; i++) {
    std::list<std::string> errors;
    manager.refresh(errors);
  }
  EXPECT_EQ(manager.entries_v4.size(), 1u);
}

TEST(AllowedHostsTest, AnOlderRefreshDoesNotOverwriteANewerOne) {
  // Overlapping refreshes: one that started before another already published
  // drops its result, so a slow, failing lookup cannot bring back addresses
  // the newer refresh removed. Simulated by marking a later refresh as
  // published before this one starts.
  allowed_hosts_manager manager;
  manager.set_source("127.0.0.1");
  manager.entries_v4.emplace_back("removed", make_address("192.0.2.7").to_v4().to_bytes(), allowed_hosts_manager::addr_v4{{255, 255, 255, 255}});
  manager.refresh_published_ = manager.refresh_started_ + 2;
  std::list<std::string> errors;
  manager.refresh(errors);
  ASSERT_EQ(manager.entries_v4.size(), 1u);
  EXPECT_EQ(manager.entries_v4.front().host, "removed");
  // The next one is newer than anything published, and goes in.
  manager.refresh(errors);
  manager.refresh(errors);
  EXPECT_TRUE(manager.is_allowed(make_address("127.0.0.1"), errors));
  EXPECT_FALSE(manager.is_allowed(make_address("192.0.2.7"), errors));
}

TEST(AllowedHostsTest, SetSourceDiscardsARefreshAlreadyUnderWay) {
  allowed_hosts_manager manager;
  manager.set_source("127.0.0.1");
  // What refresh() does as it takes its snapshot.
  const std::uint64_t in_flight = ++manager.refresh_started_;
  manager.set_source("10.0.0.1");
  EXPECT_LT(in_flight, manager.refresh_published_);
}

TEST(AllowedHostsTest, TheBackgroundRefreshResolvesAndStops) {
  allowed_hosts_manager manager;
  manager.set_source("127.0.0.1,no-such-host.invalid");
  std::mutex mutex;
  int reports = 0;
  manager.start_background_refresh(std::chrono::seconds(1), [&](const std::list<std::string> &errors) {
    const std::lock_guard<std::mutex> lock(mutex);
    if (!errors.empty()) reports++;
  });
  std::list<std::string> errors;
  for (int i = 0; i < 100 && !manager.is_allowed(make_address("127.0.0.1"), errors); i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  EXPECT_TRUE(manager.is_allowed(make_address("127.0.0.1"), errors));
  const std::shared_ptr<boost::thread> thread = manager.stop_background_refresh();
  ASSERT_TRUE(thread);
  thread->join();
  int seen = 0;
  {
    const std::lock_guard<std::mutex> lock(mutex);
    seen = reports;
  }
  // The failing name was reported, and nothing is reported once stopped.
  EXPECT_GE(seen, 1);
  EXPECT_FALSE(manager.stop_background_refresh());
}
