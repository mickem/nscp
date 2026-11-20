// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_execute.hpp"

#include <gtest/gtest.h>

namespace json = boost::json;
using namespace icinga_agent;

TEST(IcingaAgentExecute, QueryName) {
  EXPECT_EQ(query_name("nscp-disk", json::object{{"ifw_api_command", "check_drivesize"}}), "check_drivesize");
  EXPECT_EQ(query_name("check_uptime", json::object()), "check_uptime");
  EXPECT_EQ(query_name("check_uptime", json::object{{"ifw_api_command", ""}}), "check_uptime");
}

TEST(IcingaAgentExecute, ParsesAnIfwApiExecuteCommand) {
  const json::value params = json::parse(R"({
    "command_type": "check_command", "command": "nscp-cpu", "host": "win1", "service": "cpu", "deadline": 1794000000.5,
    "macros": {
      "ifw_api_command": "check_cpu",
      "ifw_api_arguments": {"warning": {"value": "$cpu_warn$"}, "time": {"value": "$cpu_time$"}},
      "cpu_warn": "load > 80", "cpu_time": ["1m", "5m"]
    }})");
  const query_request req = parse_execute_command(params);
  EXPECT_TRUE(req.error.empty()) << req.error;
  EXPECT_EQ(req.query, "check_cpu");
  EXPECT_EQ(req.icinga_command, "nscp-cpu");
  EXPECT_EQ(req.host, "win1");
  EXPECT_EQ(req.service, "cpu");
  EXPECT_DOUBLE_EQ(req.deadline, 1794000000.5);
  const std::list<std::string> expected{"time=1m", "time=5m", "warning=load > 80"};
  EXPECT_EQ(req.arguments, expected);
}

TEST(IcingaAgentExecute, APlainCommandRunsByItsName) {
  const query_request req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"check_uptime","host":"h","macros":{}})"));
  EXPECT_TRUE(req.error.empty());
  EXPECT_EQ(req.query, "check_uptime");
  EXPECT_TRUE(req.arguments.empty());
  EXPECT_TRUE(req.service.empty());
}

TEST(IcingaAgentExecute, RefusesEventCommands) {
  const query_request req = parse_execute_command(json::parse(R"({"command_type":"event_command","command":"restart","host":"h"})"));
  EXPECT_NE(req.error.find("Only check commands"), std::string::npos);
}

TEST(IcingaAgentExecute, ReportsBrokenInputAsAnError) {
  EXPECT_FALSE(parse_execute_command(json::value(nullptr)).error.empty());
  EXPECT_FALSE(parse_execute_command(json::parse(R"({"command_type":"check_command"})")).error.empty());
  // A required argument whose macro is missing.
  const query_request req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"x","macros":{
      "ifw_api_command":"check_ok","ifw_api_arguments":{"message":{"value":"$m$","required":true}}}})"));
  EXPECT_NE(req.error.find("Failed to build arguments"), std::string::npos);
}

TEST(IcingaAgentExecute, IgnoresMacrosOfTheWrongType) {
  const query_request req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"check_ok","macros":"nonsense"})"));
  EXPECT_TRUE(req.error.empty());
  EXPECT_EQ(req.query, "check_ok");
}
