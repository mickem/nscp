// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <string>

#include "mysql_client.hpp"

namespace mysql_options {

// Shared connection options for all CheckMySQL commands. `info` arrives
// pre-populated with the module's /settings/mysql defaults, so options only
// override what the user passes explicitly.
//
// Three connection parameters are deliberately NOT here, and are settable only
// through /settings/mysql (nsclient.ini, admin-only on the modern layout):
//
//   plugin-dir    - the directory libmariadb LoadLibrary/dlopen's client auth
//                   plugins from during connect (MySQL 8's caching_sha2_password
//                   is one). A per-request value is arbitrary code execution as
//                   the service account: `plugin-dir=\\attacker\share` loads and
//                   runs a DLL off an SMB share inside the (LocalSystem) agent.
//   socket        - a Windows named pipe can be a UNC path (\\host\pipe\x),
//                   making the service authenticate outbound over SMB - the
//                   credential-relay variant of the same problem.
//   defaults-file - an arbitrary file the connector reads as the service
//                   account (credential disclosure).
//
// None of the three is per-target the way host/port/database are; they are
// deployment properties. Because parse_options() rejects any option not
// registered here (boost throws unknown_option), leaving them out means a REST
// caller who sends plugin-dir=... gets "Invalid command line", not a silent
// load - the allowlist is the option table itself. See the CheckDocker
// is_local_docker_endpoint() guard for the same UNC/SMB reasoning.
inline void add_connection_options(boost::program_options::options_description &desc, mysql_client::connection_info &info) {
  namespace po = boost::program_options;
  // clang-format off
  desc.add_options()
    ("host", po::value<std::string>(&info.host)->default_value(info.host), "MySQL/MariaDB server to connect to.")
    ("port", po::value<int>(&info.port)->default_value(info.port), "TCP port of the server.")
    ("database", po::value<std::string>(&info.database), "Default database (schema) to connect to.")
    ("user", po::value<std::string>(&info.user), "User to authenticate with.")
    ("password", po::value<std::string>(&info.password), "Password to authenticate with.")
    ("tls", po::value<bool>(&info.tls)->implicit_value(true)->default_value(info.tls), "Require TLS on the connection.")
    ("timeout", po::value<int>(&info.connect_timeout)->default_value(info.connect_timeout), "Connection timeout in seconds.")
    ("query-timeout", po::value<int>(&info.query_timeout)->default_value(info.query_timeout), "Query (read/write) timeout in seconds.")
    ;
  // clang-format on
}

// set_response_bad appends, so a failure raised after post_process() has
// already written the result line would produce a garbled two-line UNKNOWN.
// Drop anything already rendered so the error is the only thing reported.
inline void fail(PB::Commands::QueryResponseMessage::Response *response, const std::string &message) {
  response->clear_lines();
  nscapi::protobuf::functions::set_response_bad(*response, message);
}

// Connect via the factory and run `body(runner)` with the module's stable
// error contract: connect failures become "Failed to connect to MySQL server
// '<target>': ...", query failures "Query failed: ..." and anything else
// raised while running the check "Check failed: ..." — all UNKNOWN.
template <class TBody>
void with_runner(const mysql_client::session_factory &factory, const mysql_client::connection_info &info,
                 PB::Commands::QueryResponseMessage::Response *response, TBody body) {
  try {
    mysql_client::query_runner run;
    try {
      run = factory(info);
    } catch (const mysql_client::mysql_exception &e) {
      return fail(response, "Failed to connect to MySQL server '" + info.display_target() + "': " + e.reason());
    }
    try {
      body(run);
    } catch (const mysql_client::mysql_exception &e) {
      return fail(response, "Query failed: " + e.reason());
    } catch (const std::exception &e) {
      // The connection succeeded, so this is a rendering/threshold failure,
      // not a connect failure; label it accordingly instead of misdirecting
      // the operator to the network.
      return fail(response, std::string("Check failed: ") + e.what());
    }
  } catch (const std::exception &e) {
    // Only factory/session construction can reach here.
    return fail(response, std::string("Failed to connect to MySQL server: ") + e.what());
  }
}

}  // namespace mysql_options
