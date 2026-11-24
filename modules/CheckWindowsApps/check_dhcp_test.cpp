// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Pure-logic tests for the CheckWindowsApps DHCP Server checks: the address and state helpers, the scope
// and server records, and both checks run end to end over canned data with
// REST-style key=value arguments.

#include "check_dhcp.hpp"

#include <gtest/gtest.h>

#include <map>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>
#include <vector>

#include "dhcp_filter.hpp"

// Normally provided by NSC_WRAP_DLL() in the auto-generated module.cpp; in the
// test binary there is no generated module, so define the singleton here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

using dhcp_filter::make_scope;
using dhcp_filter::scope_obj_ptr;
using dhcp_filter::scope_stats;

const std::uint32_t kNet192 = 0xC0A80100;  // 192.168.1.0
const std::uint32_t kNet10 = 0x0A000000;   // 10.0.0.0
const std::uint32_t kMask24 = 0xFFFFFF00;  // 255.255.255.0
const std::uint32_t kMask16 = 0xFFFF0000;  // 255.255.0.0

scope_obj_ptr scope(const std::uint32_t net, const std::uint32_t mask, const std::string &name, const int state, const long long in_use,
                    const long long available, const long long pending = 0) {
  scope_stats stats;
  stats.in_use = in_use;
  stats.available = available;
  stats.pending_offers = pending;
  return make_scope(net, mask, name, "", state, &stats);
}

struct result {
  PB::Common::ResultCode code;
  std::string message;
  std::string perf;  // "label=value;warn;crit ..." for every perf entry
};

result collect(const PB::Commands::QueryResponseMessage::Response &response) {
  result r;
  r.code = response.result();
  for (const auto &line : response.lines()) {
    r.message += line.message();
    for (const auto &p : line.perf()) {
      r.perf += p.alias() + "=";
      if (p.has_float_value()) {
        r.perf += std::to_string(p.float_value().value());
        if (p.float_value().has_warning()) r.perf += ";w" + std::to_string(p.float_value().warning().value());
        if (p.float_value().has_critical()) r.perf += ";c" + std::to_string(p.float_value().critical().value());
      }
      r.perf += " ";
    }
  }
  return r;
}

PB::Commands::QueryRequestMessage::Request make_request(const std::string &command, const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

result run_scopes(const std::vector<std::string> &args, const std::vector<scope_obj_ptr> &scopes) {
  const auto request = make_request("check_dhcp_scopes", args);
  PB::Commands::QueryResponseMessage::Response response;
  check_dhcp::check_dhcp_scopes(request, &response, [&scopes](std::vector<scope_obj_ptr> &out, std::string &) {
    out = scopes;
    return true;
  });
  return collect(response);
}

result run_server(const std::vector<std::string> &args, const std::map<std::string, double> &counters) {
  const auto request = make_request("check_dhcp_server", args);
  PB::Commands::QueryResponseMessage::Response response;
  check_dhcp::check_dhcp_server(request, &response, [&counters](std::map<std::string, double> &out, std::string &) {
    out = counters;
    return true;
  });
  return collect(response);
}

// --- helpers ----------------------------------------------------------------

TEST(DhcpHelpers, FormatsHostOrderAddresses) {
  EXPECT_EQ("192.168.1.0", dhcp_filter::format_ipv4(kNet192));
  EXPECT_EQ("10.0.0.0", dhcp_filter::format_ipv4(kNet10));
  EXPECT_EQ("255.255.255.0", dhcp_filter::format_ipv4(kMask24));
  EXPECT_EQ("0.0.0.0", dhcp_filter::format_ipv4(0));
  EXPECT_EQ("255.255.255.255", dhcp_filter::format_ipv4(0xFFFFFFFF));
}

TEST(DhcpHelpers, PrefixLength) {
  EXPECT_EQ(24, dhcp_filter::prefix_length(kMask24));
  EXPECT_EQ(16, dhcp_filter::prefix_length(kMask16));
  EXPECT_EQ(32, dhcp_filter::prefix_length(0xFFFFFFFF));
  EXPECT_EQ(0, dhcp_filter::prefix_length(0));
  EXPECT_EQ(22, dhcp_filter::prefix_length(0xFFFFFC00));
}

TEST(DhcpHelpers, SubnetStates) {
  EXPECT_EQ("enabled", dhcp_filter::subnet_state_name(0));
  EXPECT_EQ("disabled", dhcp_filter::subnet_state_name(1));
  EXPECT_EQ("enabled_switched", dhcp_filter::subnet_state_name(2));
  EXPECT_EQ("disabled_switched", dhcp_filter::subnet_state_name(3));
  EXPECT_EQ("invalid", dhcp_filter::subnet_state_name(4));
  EXPECT_EQ("invalid", dhcp_filter::subnet_state_name(-1));
  EXPECT_TRUE(dhcp_filter::subnet_state_enabled(0));
  EXPECT_FALSE(dhcp_filter::subnet_state_enabled(1));
  EXPECT_TRUE(dhcp_filter::subnet_state_enabled(2));
  EXPECT_FALSE(dhcp_filter::subnet_state_enabled(3));
  EXPECT_FALSE(dhcp_filter::subnet_state_enabled(4));
}

// --- scope records ----------------------------------------------------------

TEST(DhcpScope, DerivesAddressesAndPercentages) {
  const scope_obj_ptr s = scope(kNet192, kMask24, "Office", 0, 150, 50, 3);
  EXPECT_EQ("192.168.1.0/24", s->get_scope());
  EXPECT_EQ("255.255.255.0", s->mask);
  EXPECT_EQ(24, s->prefix);
  EXPECT_EQ("enabled", s->state);
  EXPECT_TRUE(s->enabled);
  EXPECT_TRUE(s->stats_known);
  EXPECT_EQ(200, s->get_addresses());
  EXPECT_DOUBLE_EQ(75.0, s->get_used_pct());
  EXPECT_DOUBLE_EQ(25.0, s->get_free_pct());
  EXPECT_EQ(3, s->pending_offers);
}

TEST(DhcpScope, EmptyScopeIsNeitherUsedNorFree) {
  const scope_obj_ptr s = scope(kNet192, kMask24, "Excluded", 0, 0, 0);
  EXPECT_EQ(0, s->get_addresses());
  EXPECT_DOUBLE_EQ(0.0, s->get_used_pct());
  EXPECT_DOUBLE_EQ(0.0, s->get_free_pct());
}

TEST(DhcpScope, MissingStatisticsAreFlagged) {
  const scope_obj_ptr s = make_scope(kNet10, kMask16, "New", "just created", 1, nullptr);
  EXPECT_FALSE(s->stats_known);
  EXPECT_FALSE(s->enabled);
  EXPECT_EQ("disabled", s->state);
  EXPECT_EQ("just created", s->comment);
  EXPECT_EQ(0, s->get_addresses());
}

// --- check_dhcp_scopes ------------------------------------------------------

TEST(CheckDhcpScopes, OkBelowDefaultThresholds) {
  const result r = run_scopes({}, {scope(kNet192, kMask24, "Office", 0, 100, 100), scope(kNet10, kMask16, "Lab", 0, 10, 990)});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_EQ("OK: all 2 scopes are below their thresholds", r.message);
}

TEST(CheckDhcpScopes, WarnsAbove80Percent) {
  const result r = run_scopes({}, {scope(kNet192, kMask24, "Office", 0, 170, 30), scope(kNet10, kMask16, "Lab", 0, 10, 990)});
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("192.168.1.0/24 (Office): 85% used, 30 of 200 free")) << r.message;
  EXPECT_EQ(std::string::npos, r.message.find("Lab")) << r.message;
}

TEST(CheckDhcpScopes, CriticalAbove95Percent) {
  const result r = run_scopes({}, {scope(kNet192, kMask24, "Office", 0, 199, 1)});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("192.168.1.0/24")) << r.message;
}

TEST(CheckDhcpScopes, PerfdataForTheThresholdedKeyword) {
  const result r = run_scopes({}, {scope(kNet192, kMask24, "Office", 0, 150, 50)});
  EXPECT_NE(std::string::npos, r.perf.find("192.168.1.0/24_used_pct=75")) << r.perf;
}

TEST(CheckDhcpScopes, DisabledScopesAreIgnoredByDefault) {
  const result r = run_scopes({}, {scope(kNet192, kMask24, "Retired", 1, 200, 0), scope(kNet10, kMask16, "Lab", 0, 10, 990)});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_EQ("OK: all 1 scopes are below their thresholds", r.message);
}

TEST(CheckDhcpScopes, DisabledScopesCanBeIncluded) {
  const result r = run_scopes({"filter=none"}, {scope(kNet192, kMask24, "Retired", 1, 200, 0)});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
}

TEST(CheckDhcpScopes, NoEnabledScopesIsUnknown) {
  const result r = run_scopes({}, {scope(kNet192, kMask24, "Retired", 1, 10, 190)});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_EQ("No enabled DHCP scopes found", r.message);
}

TEST(CheckDhcpScopes, EmptyStateCanBeOverridden) {
  const result r = run_scopes({"empty-state=ok"}, {});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
}

TEST(CheckDhcpScopes, EmptyScopeNeverAlerts) {
  const result r = run_scopes({}, {scope(kNet192, kMask24, "Excluded", 0, 0, 0)});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
}

TEST(CheckDhcpScopes, ShortThresholdsBoundUsedPercent) {
  const std::vector<scope_obj_ptr> scopes = {scope(kNet192, kMask24, "Office", 0, 120, 80)};  // 60%
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_scopes({"w=50", "c=70"}, scopes).code);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_scopes({"w=40", "c=55"}, scopes).code);
  EXPECT_EQ(PB::Common::ResultCode::OK, run_scopes({"w=70", "c=90"}, scopes).code);
}

TEST(CheckDhcpScopes, FreeAddressThreshold) {
  // A big scope at 90% still has plenty left; thresholding on free addresses
  // instead of the percentage is the alternative the docs describe.
  const std::vector<scope_obj_ptr> scopes = {scope(kNet10, kMask16, "Campus", 0, 9000, 1000)};
  const result r = run_scopes({"warning=free < 100", "critical=free < 20"}, scopes);
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  const result low = run_scopes({"warning=free < 100", "critical=free < 20"}, {scope(kNet192, kMask24, "Office", 0, 190, 10)});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, low.code) << low.message;
}

TEST(CheckDhcpScopes, FilterByScopeAndName) {
  const std::vector<scope_obj_ptr> scopes = {scope(kNet192, kMask24, "Office", 0, 199, 1), scope(kNet10, kMask16, "Lab", 0, 10, 990)};
  EXPECT_EQ(PB::Common::ResultCode::OK, run_scopes({"filter=scope = '10.0.0.0/16'"}, scopes).code);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_scopes({"filter=name = 'Office'"}, scopes).code);
}

TEST(CheckDhcpScopes, FetchErrorIsUnknown) {
  const auto request = make_request("check_dhcp_scopes", {});
  PB::Commands::QueryResponseMessage::Response response;
  check_dhcp::check_dhcp_scopes(request, &response, [](std::vector<scope_obj_ptr> &, std::string &error) {
    error = "DHCP Server service is not available on this host";
    return false;
  });
  const result r = collect(response);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("DHCP Server service is not available on this host", r.message);
}

// --- check_dhcp_server ------------------------------------------------------

std::map<std::string, double> quiet_server() {
  return {{"Packets Received/sec", 4.0}, {"Discovers/sec", 1.0}, {"Offers/sec", 1.0},         {"Requests/sec", 2.0}, {"Acks/sec", 2.0}, {"Nacks/sec", 0.0},
          {"Declines/sec", 0.0},         {"Releases/sec", 0.0},  {"Active Queue Length", 0.0}};
}

TEST(DhcpServer, MapsEveryCounterToItsField) {
  std::map<std::string, double> counters;
  double v = 1.0;
  for (std::size_t i = 0; i < dhcp_filter::kServerCounterCount; ++i) counters[dhcp_filter::kServerCounters[i].counter] = v++;
  const dhcp_filter::server_obj_ptr s = dhcp_filter::make_server(counters);
  for (std::size_t i = 0; i < dhcp_filter::kServerCounterCount; ++i)
    EXPECT_DOUBLE_EQ(static_cast<double>(i + 1), (*s).*(dhcp_filter::kServerCounters[i].field)) << dhcp_filter::kServerCounters[i].keyword;
}

TEST(DhcpServer, MissingCountersReadZero) {
  const dhcp_filter::server_obj_ptr s = dhcp_filter::make_server({{"Acks/sec", 3.5}});
  EXPECT_DOUBLE_EQ(3.5, s->acks);
  EXPECT_DOUBLE_EQ(0.0, s->nacks);
  EXPECT_DOUBLE_EQ(0.0, s->conflict_queue);
}

TEST(CheckDhcpServer, QuietServerIsOk) {
  const result r = run_server({}, quiet_server());
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("1 discovers/s")) << r.message;
  EXPECT_NE(std::string::npos, r.message.find("2 acks/s")) << r.message;
  // Every counter is graphed, thresholded or not.
  for (std::size_t i = 0; i < dhcp_filter::kServerCounterCount; ++i)
    EXPECT_NE(std::string::npos, r.perf.find(std::string("_") + dhcp_filter::kServerCounters[i].keyword + "=")) << r.perf;
}

TEST(CheckDhcpServer, DeclinesWarn) {
  std::map<std::string, double> c = quiet_server();
  c["Declines/sec"] = 1.0;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_server({}, c).code);
}

TEST(CheckDhcpServer, SustainedNaksWarn) {
  std::map<std::string, double> c = quiet_server();
  c["Nacks/sec"] = 5.0;
  EXPECT_EQ(PB::Common::ResultCode::OK, run_server({}, c).code);
  c["Nacks/sec"] = 15.0;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_server({}, c).code);
}

TEST(CheckDhcpServer, QueueBacklogIsCritical) {
  std::map<std::string, double> c = quiet_server();
  c["Active Queue Length"] = 60.0;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_server({}, c).code);
  c["Active Queue Length"] = 250.0;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_server({}, c).code);
}

TEST(CheckDhcpServer, ShortThresholdsBoundTheQueueAndKeepTheOtherConditions) {
  std::map<std::string, double> c = quiet_server();
  c["Active Queue Length"] = 15.0;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_server({"w=10", "c=20"}, c).code);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run_server({"w=5", "c=10"}, c).code);
  c["Active Queue Length"] = 0.0;
  c["Declines/sec"] = 2.0;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run_server({"w=10", "c=20"}, c).code);
}

TEST(CheckDhcpServer, FetchErrorIsUnknown) {
  const auto request = make_request("check_dhcp_server", {});
  PB::Commands::QueryResponseMessage::Response response;
  check_dhcp::check_dhcp_server(request, &response, [](std::map<std::string, double> &, std::string &error) {
    error = "DHCP Server performance counters are not available";
    return false;
  });
  const result r = collect(response);
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("DHCP Server performance counters are not available", r.message);
}

}  // namespace
