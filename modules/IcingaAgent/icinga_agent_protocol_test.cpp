// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_protocol.hpp"

#include <gtest/gtest.h>

namespace json = boost::json;
using namespace icinga_agent;

TEST(IcingaAgentNetstring, SplitsAndBuffers) {
  netstring_reader r;
  const std::string a = frame(R"({"a":1})");
  const std::string b = frame(R"({"b":2})");
  const std::string wire = a + b;
  auto out = r.feed(wire.data(), 5);
  EXPECT_TRUE(out.empty());
  out = r.feed(wire.data() + 5, wire.size() - 5);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0], R"({"a":1})");
  EXPECT_EQ(out[1], R"({"b":2})");
  EXPECT_EQ(r.buffered(), 0u);
}

TEST(IcingaAgentNetstring, RejectsGarbage) {
  netstring_reader r;
  EXPECT_THROW(r.feed("x:abc,", 6), std::runtime_error);
  netstring_reader r2(10);
  EXPECT_THROW(r2.feed("99:", 3), std::runtime_error);
  netstring_reader r3;
  EXPECT_THROW(r3.feed("3:abcX", 6), std::runtime_error);
}

TEST(IcingaAgentProtocol, FrameUsesByteLength) { EXPECT_EQ(frame("\xc3\xa5"), "2:\xc3\xa5,"); }

TEST(IcingaAgentProtocol, ExpandsMacros) {
  json::object m{{"a", "x"}, {"n", 5}, {"list", json::array{"p", "q"}}};
  EXPECT_EQ(expand_macros("$a$", m), json::value("x"));
  EXPECT_EQ(expand_macros("$n$", m), json::value(5));
  EXPECT_EQ(expand_macros("$list$", m), (json::value(json::array{"p", "q"})));
  EXPECT_EQ(expand_macros("load>$n$ $$", m), json::value("load>5 $"));
  EXPECT_TRUE(expand_macros("$missing$", m).is_null());
}

TEST(IcingaAgentProtocol, BuildsArgumentsLikeIcinga) {
  const json::object macros{{"warn", "load > 80"}, {"crit", ""}, {"show", true}, {"hide", false}, {"filters", json::array{"a", "b"}}};
  const json::object args{
      {"warning", json::object{{"value", "$warn$"}, {"order", 1}}},
      {"critical", json::object{{"value", "$crit$"}, {"order", 2}}},     // empty -> dropped
      {"--show-all", json::object{{"set_if", "$show$"}, {"order", 3}}},  // flag, dashes stripped
      {"hidden", json::object{{"set_if", "$hide$"}}},                    // set_if false -> dropped
      {"missing", json::object{{"value", "$nope$"}}},                    // macro not sent -> dropped
      {"filter", json::object{{"value", "$filters$"}, {"order", 4}}},    // repeated key
      {"positional", json::object{{"value", "x"}, {"skip_key", true}, {"order", 5}}},
  };
  const auto out = build_arguments(args, macros);
  const std::list<std::string> expected{"warning=load > 80", "show-all", "filter=a", "filter=b", "x"};
  EXPECT_EQ(out, expected);
}

TEST(IcingaAgentProtocol, ParsesIfwApiExecuteCommand) {
  const json::value params = json::parse(R"({
    "command_type": "check_command", "command": "nscp-cpu", "host": "win1", "service": "cpu",
    "macros": {
      "ifw_api_command": "check_cpu",
      "ifw_api_arguments": {"warning": {"value": "$cpu_warn$"}, "time": {"value": "$cpu_time$", "repeat_key": true}},
      "cpu_warn": "load > 80", "cpu_time": ["1m", "5m"]
    }})");
  const auto req = parse_execute_command(params);
  EXPECT_TRUE(req.error.empty()) << req.error;
  EXPECT_EQ(req.query, "check_cpu");
  EXPECT_EQ(req.host, "win1");
  EXPECT_EQ(req.service, "cpu");
  const std::list<std::string> expected{"time=1m", "time=5m", "warning=load > 80"};
  EXPECT_EQ(req.arguments, expected);
}

TEST(IcingaAgentProtocol, FallsBackToCommandName) {
  const auto req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"check_uptime","host":"h","macros":{}})"));
  EXPECT_TRUE(req.error.empty());
  EXPECT_EQ(req.query, "check_uptime");
  EXPECT_TRUE(req.arguments.empty());
}

TEST(IcingaAgentProtocol, RefusesEventCommands) {
  const auto req = parse_execute_command(json::parse(R"({"command_type":"event_command","command":"restart","host":"h"})"));
  EXPECT_FALSE(req.error.empty());
}

TEST(IcingaAgentProtocol, CheckResultShape) {
  check_result r;
  r.exit_status = 1;
  r.output = "WARNING: x";
  r.perfdata = "'a b'=1;2;3 c=4%";
  const std::string framed = make_check_result("h", "s", r);
  netstring_reader reader;
  const auto msgs = reader.feed(framed.data(), framed.size());
  ASSERT_EQ(msgs.size(), 1u);
  const auto v = json::parse(msgs[0]);
  EXPECT_EQ(v.at("method"), "event::CheckResult");
  EXPECT_EQ(v.at("params").at("service"), "s");
  const auto &cr = v.at("params").at("cr");
  EXPECT_EQ(cr.at("exit_status"), 1);
  ASSERT_EQ(cr.at("performance_data").as_array().size(), 2u);
  EXPECT_EQ(cr.at("performance_data").as_array()[0], "'a b'=1;2;3");
}

TEST(IcingaAgentProtocol, NormalizesFingerprints) {
  const std::string hex = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  std::string out;
  ASSERT_TRUE(normalize_fingerprint(hex, out));
  EXPECT_EQ(out, hex);
  // What `openssl x509 -noout -fingerprint -sha256` prints.
  ASSERT_TRUE(normalize_fingerprint("SHA256 Fingerprint=01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF", out));
  EXPECT_EQ(out, hex);
  ASSERT_TRUE(normalize_fingerprint("0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567 89AB CDEF 0123 4567 89AB CDEF", out));
  EXPECT_EQ(out, hex);
  // SHA-1 length, a stray character, nothing at all.
  EXPECT_FALSE(normalize_fingerprint("01:23:45:67:89:ab:cd:ef:01:23:45:67:89:ab:cd:ef:01:23:45:67", out));
  EXPECT_FALSE(normalize_fingerprint(hex.substr(0, 63) + "g", out));
  EXPECT_FALSE(normalize_fingerprint("", out));
}
