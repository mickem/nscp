// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <set>
#include <string>

// The pure, dispatch-free half of the legacy check_nt (NSClient) protocol:
// the `allow` setting parser, the request-code -> modern-query mapping and
// the modern-response -> legacy-payload formatting. Kept free of core/plugin
// dependencies so it can be unit tested (check_nt_commands_test.cpp);
// NSClientServer.cpp owns the socket, the password gate and the dispatch.
namespace check_nt_commands {

// Request codes from the check_nt wire protocol (`<password>&<code>&<args>`).
constexpr int REQ_CLIENTVERSION = 1;
constexpr int REQ_CPULOAD = 2;
constexpr int REQ_UPTIME = 3;
constexpr int REQ_USEDDISKSPACE = 4;
constexpr int REQ_SERVICESTATE = 5;
constexpr int REQ_PROCSTATE = 6;
constexpr int REQ_MEMUSE = 7;
constexpr int REQ_COUNTER = 8;
constexpr int REQ_FILEAGE = 9;
constexpr int REQ_INSTANCES = 10;

// Resolve the `allow` setting - a comma separated list of groups, the
// keyword "any"/"all", or individual command names - into the set of
// permitted request codes. Unrecognised tokens are collected in `unknown`
// (the caller decides how to report them); they enable nothing, so a spec
// of only unknown tokens yields an empty set and the server fails closed.
std::set<int> parse_allowed_commands(const std::string &spec, std::set<std::string> &unknown);

// A check_nt request code mapped onto the modern query it is served by.
struct mapped_command {
  std::string command;
  std::list<std::string> arguments;
};

// Map a request code plus its raw `&`-separated argument string onto the
// modern query to dispatch. Returns false for the codes that are not backed
// by a query (REQ_CLIENTVERSION and REQ_INSTANCES are answered inline by the
// server) and for unknown codes.
bool map_request(int code, const std::string &raw_args, mapped_command &out);

// Render the single response payload of the dispatched query back into the
// legacy check_nt reply for `code`. `command` is only used in error texts.
// Returns an "ERROR: ..." payload when the response does not carry what the
// legacy format needs (wrong line count, missing performance data).
std::string format_response(int code, const std::string &command, const PB::Commands::QueryResponseMessage::Response &payload);

}  // namespace check_nt_commands
