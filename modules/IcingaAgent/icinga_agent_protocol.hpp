// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The Icinga 2 cluster protocol, the part of it an agent needs: framing,
// the messages an agent sends, and turning an `event::ExecuteCommand` into an
// NSClient++ query. Nothing in here touches a socket or the core, so it unit
// tests without either.
//
// Wire format (lib/remote/jsonrpc.cpp, lib/base/netstring.cpp in Icinga 2):
// every message is a JSON-RPC 2.0 object framed as a netstring,
// `<decimal byte length>:<json>,`.

#include <boost/json.hpp>
#include <list>
#include <string>
#include <vector>

namespace icinga_agent {

// The version we announce in icinga::Hello. Icinga 2 uses it to decide which
// optional fields a peer understands; 2.16.0 is what Icinga for Windows
// announces as well.
constexpr int kAnnouncedVersion = 21600;

// Incremental netstring decoder. Feed it whatever the socket produced; it
// returns every complete payload and keeps the remainder.
class netstring_reader {
 public:
  explicit netstring_reader(std::size_t max_message = 64 * 1024 * 1024) : max_message_(max_message) {}
  // Throws std::runtime_error on a malformed stream (the connection must be
  // dropped: there is no way to resynchronise a netstring stream).
  std::vector<std::string> feed(const char *data, std::size_t size);
  std::size_t buffered() const { return buffer_.size(); }

 private:
  std::string buffer_;
  std::size_t max_message_;
};

std::string frame(const std::string &json);
std::string make_message(const std::string &method, boost::json::object params);

std::string make_hello(const std::string &agent_version);
std::string make_heartbeat(int timeout_seconds);
std::string make_log_position(double unix_time);
std::string make_certificate_request(const std::string &ticket);

// What an ExecuteCommand turns into on our side.
struct query_request {
  std::string host;
  std::string service;         // empty for a host check
  std::string command_type;    // "check_command" / "event_command"
  std::string icinga_command;  // params.command, the CheckCommand object name
  std::string query;           // the NSClient++ query to run
  std::list<std::string> arguments;
  double deadline = 0;  // 0 when the master did not send one
  std::string error;    // non-empty: do not run, answer UNKNOWN with this
};

// Resolve an `event::ExecuteCommand` params object.
//
// Commands follow the convention Icinga for Windows introduced with the
// `ifw-api` CheckCommand template (Icinga 2.14+): the master sends
// `macros.ifw_api_command` (what to run) and `macros.ifw_api_arguments` (the
// CheckCommand's `arguments` block, values still holding `$macro$`
// references) together with the resolved values of every macro those
// arguments reference. We expand them the way Icinga's own argument
// processing does and hand the result to the core as `key=value` tokens.
//
// Without `ifw_api_command` the CheckCommand object name is used as the query
// name with no arguments, so `object CheckCommand "check_cpu" { import
// "plugin-check-command" ... }` keeps working for argument-less checks.
query_request parse_execute_command(const boost::json::value &params);

// Expand `$name$` references in `text` against `macros`. A string that is
// exactly one macro keeps the macro's JSON type (an array stays an array).
boost::json::value expand_macros(const std::string &text, const boost::json::object &macros);

// Build the NSClient++ argument list from an ifw_api_arguments dictionary.
std::list<std::string> build_arguments(const boost::json::object &arguments, const boost::json::object &macros);

struct check_result {
  int exit_status = 3;
  std::string output;
  std::string perfdata;
  double execution_start = 0;
  double execution_end = 0;
  std::string check_source;
  std::string command_line;
};

std::string make_check_result(const std::string &host, const std::string &service, const check_result &result);

// Normalise a certificate fingerprint as an operator writes it - the output
// of `openssl x509 -noout -fingerprint -sha256` ("SHA256 Fingerprint=AB:CD:.."),
// with or without the prefix, colons or spaces, in either case - to 64
// lower-case hex digits. False when it is not a SHA-256 fingerprint.
bool normalize_fingerprint(const std::string &text, std::string &out);

// Split rendered perfdata into the array Icinga 2 expects.
std::vector<std::string> split_perfdata(const std::string &perfdata);

}  // namespace icinga_agent
