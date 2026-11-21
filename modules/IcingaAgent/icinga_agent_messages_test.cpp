// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_messages.hpp"

#include <gtest/gtest.h>

#include "icinga_agent_netstring.hpp"

namespace json = boost::json;
using namespace icinga_agent;

namespace {
// Unframe one message and parse it.
json::value decode(const std::string &framed) {
  netstring_reader reader;
  const auto msgs = reader.feed(framed.data(), framed.size());
  EXPECT_EQ(msgs.size(), 1u);
  return msgs.empty() ? json::value() : json::parse(msgs[0]);
}
}  // namespace

TEST(IcingaAgentMessages, Hello) {
  const json::value v = decode(make_hello("NSClient++/1.0"));
  EXPECT_EQ(v.at("jsonrpc"), "2.0");
  EXPECT_EQ(v.at("method"), "icinga::Hello");
  EXPECT_EQ(v.at("params").at("capabilities"), kCapabilityIfwApiCheckCommand);
  EXPECT_EQ(v.at("params").at("version"), kAnnouncedVersion);
  EXPECT_EQ(v.at("params").at("agentversion"), "NSClient++/1.0");
}

TEST(IcingaAgentMessages, HelloNeverAnnouncesArbitraryCommands) {
  const json::value v = decode(make_hello("x"));
  EXPECT_EQ(v.at("params").at("capabilities").to_number<int>() & kCapabilityExecuteArbitraryCommand, 0);
}

TEST(IcingaAgentMessages, CertificateRequestCarriesTheTicketOnlyWhenThereIsOne) {
  EXPECT_EQ(decode(make_certificate_request("abc")).at("params").at("ticket"), "abc");
  EXPECT_FALSE(decode(make_certificate_request("")).at("params").as_object().contains("ticket"));
}

TEST(IcingaAgentMessages, HeartbeatAndLogPosition) {
  EXPECT_EQ(decode(make_heartbeat(120)).at("params").at("timeout"), 120);
  EXPECT_EQ(decode(make_log_position(12.5)).at("method"), "log::SetLogPosition");
}

TEST(IcingaAgentMessages, SplitPerfdata) {
  using v = std::vector<std::string>;
  EXPECT_EQ(split_perfdata("a=1 b=2"), (v{"a=1", "b=2"}));
  EXPECT_EQ(split_perfdata("'a b'=1;2;3 c=4%"), (v{"'a b'=1;2;3", "c=4%"}));
  EXPECT_EQ(split_perfdata("  a=1   b=2\nc=3 "), (v{"a=1", "b=2", "c=3"}));
  EXPECT_EQ(split_perfdata("'C:\\ used %'=4%"), v{"'C:\\ used %'=4%"});
  EXPECT_TRUE(split_perfdata("").empty());
}

TEST(IcingaAgentMessages, CheckResultShape) {
  check_result r;
  r.exit_status = 1;
  r.output = "WARNING: x";
  r.perfdata = "'a b'=1;2;3 c=4%";
  r.check_source = "agent";
  const json::value v = decode(make_check_result("h", "s", r));
  EXPECT_EQ(v.at("method"), "event::CheckResult");
  EXPECT_EQ(v.at("params").at("host"), "h");
  EXPECT_EQ(v.at("params").at("service"), "s");
  const auto &cr = v.at("params").at("cr");
  EXPECT_EQ(cr.at("exit_status"), 1);
  EXPECT_EQ(cr.at("output"), "WARNING: x");
  EXPECT_EQ(cr.at("check_source"), "agent");
  ASSERT_EQ(cr.at("performance_data").as_array().size(), 2u);
  EXPECT_EQ(cr.at("performance_data").as_array()[0], "'a b'=1;2;3");
}

TEST(IcingaAgentMessages, AHostCheckResultHasNoService) {
  const json::value v = decode(make_check_result("h", "", check_result()));
  EXPECT_FALSE(v.at("params").as_object().contains("service"));
}
