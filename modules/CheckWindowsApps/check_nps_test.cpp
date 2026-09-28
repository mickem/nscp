// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include "check_nps.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <ctime>
#include <fstream>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_nps_internal.hpp"

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace check_nps {
// Test backend linked in place of the Windows reader. Commands, filters and
// output are the production implementations, with no fixture flags in the API.
std::vector<event> fixture_events;
std::string fixture_error;
void require_nps() {}
std::vector<event> read_events(int, int, bool accounting) {
  if (!fixture_error.empty()) throw std::runtime_error(fixture_error);
  std::vector<event> out;
  for (const auto &item : fixture_events)
    if ((item.id == 6275) == accounting) out.push_back(item);
  return out;
}
std::vector<counter_value> read_counters(const std::string &, int) {
  if (!fixture_error.empty()) throw std::runtime_error(fixture_error);
  return {{"NPS Authentication Server", "Access Requests/sec", "client1", 2.5}};
}
}  // namespace check_nps
using namespace check_nps;
namespace {
PB::Commands::QueryResponseMessage::Response run(const std::string &command, const std::vector<std::string> &args = {}) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command(command);
  for (const auto &arg : args) request.add_arguments(arg);
  if (command == "check_nps_auth")
    check_nps_auth(request, &response);
  else if (command == "check_nps_accounting")
    check_nps_accounting(request, &response);
  else
    check_nps_counters(request, &response);
  return response;
}
class Nps : public ::testing::Test {
  void SetUp() override {
    fixture_events.clear();
    fixture_error.clear();
  }
};
}  // namespace
TEST_F(Nps, ParsesStructuredFieldsAndXmlEntities) {
  const auto e = parse_event(
      R"(<Event><System><Provider Name="Microsoft-Windows-Security-Auditing"/><EventID>6273</EventID></System><EventData><Data Name="ClientName">AP &amp; VPN</Data><Data Name="NetworkPolicyName">Wifi</Data><Data Name="ReasonCode">16</Data></EventData></Event>)");
  EXPECT_EQ(e.id, 6273);
  EXPECT_EQ(e.client, "AP & VPN");
  EXPECT_EQ(e.policy, "Wifi");
  EXPECT_EQ(e.reason, "16");
}
TEST_F(Nps, MissingFieldsRemainVisibleAndMalformedEventsFail) {
  const auto e = parse_event(R"(<Event><System><Provider Name="Microsoft-Windows-Security-Auditing"/><EventID>6274</EventID></System><EventData/></Event>)");
  EXPECT_EQ(e.reason, "unknown");
  EXPECT_EQ(e.client, "unknown");
  EXPECT_THROW(parse_event("<broken"), std::exception);
  EXPECT_THROW(parse_event(R"(<Event><System><Provider Name="Other"/><EventID>6273</EventID></System></Event>)"), std::exception);
}
TEST_F(Nps, PercentagesExcludeDiscardsAndAccounting) {
  for (int i = 0; i < 80; ++i) fixture_events.push_back({6272, "AP", "Wifi", "0"});
  for (int i = 0; i < 20; ++i) fixture_events.push_back({6273, "AP", "Wifi", "16"});
  fixture_events.push_back({6274, "AP", "Wifi", "48"});
  fixture_events.push_back({6275, "AP", "Wifi", "48"});
  const auto values = summarize(fixture_events, "all");
  ASSERT_EQ(values.size(), 1u);
  EXPECT_DOUBLE_EQ(values[0].reject_pct(), 20.0);
  EXPECT_EQ(values[0].requests(), 101);
  EXPECT_EQ(values[0].accounting_discards, 1);
  EXPECT_EQ(values[0].top_reason(), "16");
}
TEST_F(Nps, GroupsClientsAndPoliciesWithoutMixingTheirCounts) {
  fixture_events = {{6272, "AP1", "Wifi", "0"}, {6273, "VPN", "Remote", "16"}, {6273, "AP1", "Wifi", "16"}};
  const auto clients = summarize(fixture_events, "client");
  ASSERT_EQ(clients.size(), 2u);
  EXPECT_EQ(clients[0].decisions(), 2);
  EXPECT_EQ(clients[1].decisions(), 1);
  EXPECT_EQ(summarize(fixture_events, "policy").size(), 2u);
  EXPECT_EQ(summarize(fixture_events, "reason").size(), 2u);
}
TEST_F(Nps, MinimumTrafficSuppressesDefaultPercentageAlerts) {
  fixture_events = {{6273, "AP", "Wifi", "16"}};
  EXPECT_EQ(run("check_nps_auth").result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(run("check_nps_auth", {"min-requests=1"}).result(), PB::Common::ResultCode::CRITICAL);
}
TEST_F(Nps, QuietWindowAndValuedBoolean) {
  EXPECT_EQ(run("check_nps_auth").result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(run("check_nps_auth", {"require-traffic=true"}).result(), PB::Common::ResultCode::CRITICAL);
  EXPECT_EQ(run("check_nps_auth", {"require-traffic=false"}).result(), PB::Common::ResultCode::OK);
}
TEST_F(Nps, DiscardsAlertIndependentlyOfLowVolume) {
  fixture_events = {{6274, "AP", "Wifi", "48"}, {6275, "AP", "Wifi", "48"}};
  EXPECT_EQ(run("check_nps_auth").result(), PB::Common::ResultCode::CRITICAL);
  EXPECT_EQ(run("check_nps_accounting").result(), PB::Common::ResultCode::CRITICAL);
}
TEST_F(Nps, IncompleteOrInaccessibleDataCannotBecomeOk) {
  for (const auto &error : {"NPS role is not installed", "NPS auditing disabled", "Access denied", "Event limit exceeded"}) {
    fixture_error = error;
    EXPECT_EQ(run("check_nps_auth", {"empty-state=ok", "critical=none"}).result(), PB::Common::ResultCode::UNKNOWN);
    EXPECT_EQ(run("check_nps_accounting").result(), PB::Common::ResultCode::UNKNOWN);
  }
}
TEST_F(Nps, CounterThresholdUsesFractionalValue) {
  const auto result = run("check_nps_counters", {"warning=value > 2", "critical=value > 3"});
  EXPECT_EQ(result.result(), PB::Common::ResultCode::WARNING);
  ASSERT_GT(result.lines_size(), 0);
  EXPECT_NE(result.lines(0).message().find("2.5"), std::string::npos);
}
TEST_F(Nps, InvalidOptionsFailBeforeCollecting) {
  EXPECT_EQ(run("check_nps_auth", {"window=0"}).result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(run("check_nps_auth", {"group-by=invalid"}).result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(run("check_nps_auth", {"group-by=client", "require-traffic=true"}).result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(run("check_nps_accounting", {"require-traffic=true"}).result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(run("check_nps_counters", {"sample-ms=0"}).result(), PB::Common::ResultCode::UNKNOWN);
}

TEST_F(Nps, AccountingFileFreshnessRequiresExpectedTraffic) {
  const auto file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nps-test-%%%%-%%%%.log");
  struct cleanup {
    boost::filesystem::path file;
    ~cleanup() {
      boost::system::error_code ignored;
      boost::filesystem::remove(file, ignored);
    }
  } remove{file};
  std::ofstream(file.string()).close();
  const std::string arg = "log-file=" + file.string();
  EXPECT_EQ(run("check_nps_accounting", {arg}).result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(run("check_nps_accounting", {arg, "require-traffic=true"}).result(), PB::Common::ResultCode::CRITICAL);
  {
    std::ofstream output(file.string());
    output << "accounting record\n";
  }
  EXPECT_EQ(run("check_nps_accounting", {arg, "require-traffic=true"}).result(), PB::Common::ResultCode::OK);
  boost::filesystem::last_write_time(file, std::time(nullptr) - 1000);
  EXPECT_EQ(run("check_nps_accounting", {arg}).result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(run("check_nps_accounting", {arg, "require-traffic=true"}).result(), PB::Common::ResultCode::CRITICAL);
  boost::filesystem::remove(file);
  EXPECT_EQ(run("check_nps_accounting", {arg}).result(), PB::Common::ResultCode::CRITICAL);
}

TEST_F(Nps, DefaultWarningAndCriticalBoundaries) {
  fixture_events.assign(80, {6272, "AP", "Wifi", "0"});
  fixture_events.insert(fixture_events.end(), 20, {6273, "AP", "Wifi", "16"});
  EXPECT_EQ(run("check_nps_auth").result(), PB::Common::ResultCode::WARNING);
  fixture_events.insert(fixture_events.end(), 10, {6273, "AP", "Wifi", "16"});
  EXPECT_EQ(run("check_nps_auth").result(), PB::Common::ResultCode::CRITICAL);
}
