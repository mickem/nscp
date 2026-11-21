// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The JSON-RPC messages an agent sends, each returned framed and ready for
// the socket. The shapes follow lib/remote/jsonrpcconnection.cpp and
// lib/icinga/clusterevents.cpp in Icinga 2.

#include <boost/json.hpp>
#include <string>
#include <vector>

namespace icinga_agent {

// The version announced in icinga::Hello. Icinga 2 uses it to decide which
// optional fields a peer understands; 2.16.0 is what Icinga for Windows
// announces as well.
constexpr int kAnnouncedVersion = 21600;

// Capability bits of icinga::Hello (lib/remote/apilistener.hpp).
constexpr int kCapabilityExecuteArbitraryCommand = 1;
constexpr int kCapabilityIfwApiCheckCommand = 2;

std::string make_message(const std::string &method, boost::json::object params);

std::string make_hello(const std::string &agent_version);
std::string make_heartbeat(int timeout_seconds);
std::string make_log_position(double unix_time);
std::string make_certificate_request(const std::string &ticket);

struct check_result {
  int exit_status = 3;
  std::string output;
  std::string perfdata;
  double execution_start = 0;
  double execution_end = 0;
  std::string check_source;
  std::string command_line;
};

// The `cr` object of an event::CheckResult.
boost::json::object check_result_object(const check_result &result);
std::string make_check_result(const std::string &host, const std::string &service, const check_result &result);

// Split rendered perfdata (`'a b'=1;2;3 c=4%`) into the array Icinga expects,
// one entry per label, keeping quoted labels with spaces together.
std::vector<std::string> split_perfdata(const std::string &perfdata);

}  // namespace icinga_agent
