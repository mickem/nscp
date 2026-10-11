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
  const std::string msg = manager.rejection_message(make_address("127.0.0.1"), errors);
  EXPECT_NE(msg.find("allowed hosts is empty"), std::string::npos) << msg;
  EXPECT_NE(msg.find("0.0.0.0/0,::/0"), std::string::npos) << msg;
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

// ---------------------------------------------------------------------------
// rejection_message(): the one line a rejected connection logs (#657).
// ---------------------------------------------------------------------------

namespace {
allowed_hosts_manager configured(const std::string &source, const std::string &path = "/settings/NRPE/server") {
  allowed_hosts_manager manager;
  manager.set_settings_path(path);
  manager.set_source(source);
  std::list<std::string> errors;
  manager.refresh(errors);
  return manager;
}
bool contains(const std::string &haystack, const std::string &needle) { return haystack.find(needle) != std::string::npos; }
}  // namespace

TEST(AllowedHostsRejection, AnAddressNotInTheListSaysWhoWhatAndWhere) {
  allowed_hosts_manager manager = configured("127.0.0.1,::1,10.0.0.0/24");
  std::list<std::string> errors;
  ASSERT_FALSE(manager.is_allowed(make_address("192.168.5.7"), errors));
  // A plain miss is not a problem with the list: the message carries the reason.
  EXPECT_TRUE(errors.empty());

  EXPECT_EQ(manager.rejection_message(make_address("192.168.5.7"), errors),
            "Rejected connection from: 192.168.5.7, not in allowed hosts (127.0.0.1, ::1, 10.0.0.0/24); set `allowed hosts` in "
            "/settings/NRPE/server, or in /settings/default if that section does not set it");
}

TEST(AllowedHostsRejection, AV4MappedClientIsNamedByItsV4Address) {
  // What a dual-stack listener reports for a v4 client; the list holds the v4 form.
  const allowed_hosts_manager manager = configured("127.0.0.1");
  const std::string msg = manager.rejection_message(make_address("::ffff:10.1.2.3"), {});
  EXPECT_TRUE(contains(msg, "Rejected connection from: 10.1.2.3,")) << msg;
}

TEST(AllowedHostsRejection, AV4MappedMissIsNotReportedAsAListProblem) {
  // It used to append "IP address not allowed" twice, once per form tried.
  allowed_hosts_manager manager = configured("127.0.0.1");
  std::list<std::string> errors;
  EXPECT_FALSE(manager.is_allowed(make_address("::ffff:10.1.2.3"), errors));
  EXPECT_TRUE(errors.empty());
}

TEST(AllowedHostsRejection, AnUnusableCachedListPointsAtTheLoadTimeErrors) {
  // The default: the module parses the list once at load and logs its errors
  // there, so an accept has nothing in `errors` to append.
  allowed_hosts_manager manager;
  manager.cached = true;
  manager.set_source("192.168.1.999");
  std::list<std::string> load_errors;
  manager.refresh(load_errors);
  ASSERT_FALSE(load_errors.empty());

  std::list<std::string> errors;
  EXPECT_FALSE(manager.is_allowed(make_address("192.168.1.1"), errors));
  EXPECT_TRUE(errors.empty());
  const std::string msg = manager.rejection_message(make_address("192.168.1.1"), errors);
  EXPECT_TRUE(contains(msg, "no entry in allowed hosts could be used (192.168.1.999)")) << msg;
  EXPECT_FALSE(contains(msg, "problems with the list")) << msg;
  EXPECT_TRUE(contains(msg, "logged when the module loaded")) << msg;
}

TEST(AllowedHostsRejection, AnUnusableUncachedListCarriesItsErrors) {
  // `cache allowed hosts = false`: is_allowed() re-reads the list on every
  // connection, so the reasons are fresh and ride the rejection line.
  allowed_hosts_manager manager;
  manager.cached = false;
  manager.set_source("192.168.1.999");

  std::list<std::string> errors;
  EXPECT_FALSE(manager.is_allowed(make_address("192.168.1.1"), errors));
  ASSERT_FALSE(errors.empty());
  const std::string msg = manager.rejection_message(make_address("192.168.1.1"), errors);
  EXPECT_TRUE(contains(msg, "no entry in allowed hosts could be used (192.168.1.999)")) << msg;
  EXPECT_TRUE(contains(msg, "; problems with the list: ")) << msg;
  EXPECT_FALSE(contains(msg, "logged when the module loaded")) << msg;
}

TEST(AllowedHostsRejection, ALongListIsCapped) {
  std::string source;
  for (int i = 1; i <= 13; i++) source += (source.empty() ? "" : ",") + std::string("10.0.0.") + std::to_string(i);
  const allowed_hosts_manager manager = configured(source);
  const std::string msg = manager.rejection_message(make_address("192.168.0.1"), {});
  EXPECT_TRUE(contains(msg, "10.0.0.10, ... (3 more))")) << msg;
  EXPECT_FALSE(contains(msg, "10.0.0.11")) << msg;
}

TEST(AllowedHostsRejection, WithoutASettingsPathTheSettingIsStillNamed) {
  const allowed_hosts_manager manager = configured("127.0.0.1", "");
  const std::string msg = manager.rejection_message(make_address("10.0.0.1"), {});
  EXPECT_TRUE(contains(msg, "set `allowed hosts` in the module's server section, or in /settings/default")) << msg;
}

TEST(AllowedHostsRejection, TheSettingsPathSurvivesACopy) {
  // connection_info, and with it the manager, is copied into each server.
  const allowed_hosts_manager manager = configured("127.0.0.1", "/settings/NSCA/server");
  const allowed_hosts_manager copy(manager);
  allowed_hosts_manager assigned;
  assigned = manager;
  EXPECT_EQ(copy.settings_path, "/settings/NSCA/server");
  EXPECT_EQ(assigned.settings_path, "/settings/NSCA/server");
}
