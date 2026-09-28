// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include "check_nps.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <ctime>
#include <fstream>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_nps_internal.hpp"
#ifdef _WIN32
#include "check_nps_event_values.hpp"
#endif

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
TEST_F(Nps, ReadsStructuredFields) {
  const auto e = make_event("Microsoft-Windows-Security-Auditing", 6273, "AP & VPN", "", "Wifi", "", "16");
  EXPECT_EQ(e.id, 6273);
  EXPECT_EQ(e.client, "AP & VPN");
  EXPECT_EQ(e.policy, "Wifi");
  EXPECT_EQ(e.reason, "16");
}
TEST_F(Nps, MissingFieldsRemainVisibleAndMalformedEventsFail) {
  const auto e = make_event("Microsoft-Windows-Security-Auditing", 6274, "", "", "", "", "");
  EXPECT_EQ(e.reason, "unknown");
  EXPECT_EQ(e.client, "unknown");
  EXPECT_EQ(e.policy, "unknown");
  EXPECT_THROW(make_event("Other", 6273, "", "", "", "", ""), std::runtime_error);
  EXPECT_THROW(make_event("Microsoft-Windows-Security-Auditing", 1, "", "", "", "", ""), std::runtime_error);
}
TEST_F(Nps, FallsBackToClientAddressAndProxyPolicy) {
  const auto e = make_event("Microsoft-Windows-Security-Auditing", 6272, "-", "192.0.2.1", "-", "Proxy", "");
  EXPECT_EQ(e.client, "192.0.2.1");
  EXPECT_EQ(e.policy, "Proxy");
  EXPECT_EQ(e.reason, "0");
}
#ifdef _WIN32
TEST_F(Nps, ReadsTypedEventValuesAndUnicodeWithoutXmlEscaping) {
  eventlog::api::EVT_VARIANT values[event_field_count]{};
  values[provider_field].Type = eventlog::api::EvtVarTypeString;
  values[provider_field].StringVal = L"Microsoft-Windows-Security-Auditing";
  values[id_field].Type = eventlog::api::EvtVarTypeUInt16;
  values[id_field].UInt16Val = 6273;
  values[client_field].Type = eventlog::api::EvtVarTypeString;
  values[client_field].StringVal = L"AP & <VPN> \u79d8\u5bc6";
  values[reason_field].Type = eventlog::api::EvtVarTypeUInt32;
  values[reason_field].UInt32Val = 16;
  const auto e = parse_event_values(values, event_field_count);
  EXPECT_EQ(e.client, utf8::cvt<std::string>(values[client_field].StringVal));
  EXPECT_EQ(e.policy, "unknown");
  EXPECT_EQ(e.reason, "16");
  values[reason_field].Type = eventlog::api::EvtVarTypeString;
  values[reason_field].StringVal = L"48";
  EXPECT_EQ(parse_event_values(values, event_field_count).reason, "48");
}
TEST_F(Nps, TypedValuesPreserveMissingFieldDefaultsAndFallbacks) {
  eventlog::api::EVT_VARIANT values[event_field_count]{};
  values[provider_field].Type = eventlog::api::EvtVarTypeString;
  values[provider_field].StringVal = L"Microsoft-Windows-Security-Auditing";
  values[id_field].Type = eventlog::api::EvtVarTypeUInt16;
  values[id_field].UInt16Val = 6272;
  const auto e = parse_event_values(values, event_field_count);
  EXPECT_EQ(e.client, "unknown");
  EXPECT_EQ(e.policy, "unknown");
  EXPECT_EQ(e.reason, "0");
  values[id_field].UInt16Val = 6275;
  values[address_field].Type = eventlog::api::EvtVarTypeString;
  values[address_field].StringVal = L"192.0.2.1";
  values[proxy_policy_field].Type = eventlog::api::EvtVarTypeString;
  values[proxy_policy_field].StringVal = L"Proxy";
  const auto fallback = parse_event_values(values, event_field_count);
  EXPECT_EQ(fallback.client, "192.0.2.1");
  EXPECT_EQ(fallback.policy, "Proxy");
  EXPECT_EQ(fallback.reason, "unknown");
}
TEST_F(Nps, RejectsIncompleteAndUnexpectedTypedValues) {
  eventlog::api::EVT_VARIANT values[event_field_count]{};
  EXPECT_THROW(parse_event_values(values, event_field_count - 1), std::runtime_error);
  EXPECT_THROW(parse_event_values(values, event_field_count), std::runtime_error);
  values[provider_field].Type = eventlog::api::EvtVarTypeString;
  values[provider_field].StringVal = L"Other";
  values[id_field].Type = eventlog::api::EvtVarTypeUInt32;
  values[id_field].UInt32Val = 6274;
  EXPECT_THROW(parse_event_values(values, event_field_count), std::runtime_error);
  values[provider_field].StringVal = L"Microsoft-Windows-Security-Auditing";
  EXPECT_EQ(parse_event_values(values, event_field_count).id, 6274);
  values[client_field].Type = eventlog::api::EvtVarTypeString | EVT_VARIANT_TYPE_ARRAY;
  EXPECT_THROW(parse_event_values(values, event_field_count), std::runtime_error);
  values[client_field].Type = eventlog::api::EvtVarTypeString;
  values[client_field].StringVal = nullptr;
  EXPECT_THROW(parse_event_values(values, event_field_count), std::runtime_error);
}
#endif
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
