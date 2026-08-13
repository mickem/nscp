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
inline void add_connection_options(boost::program_options::options_description &desc, mysql_client::connection_info &info) {
  namespace po = boost::program_options;
  // clang-format off
  desc.add_options()
    ("host", po::value<std::string>(&info.host)->default_value(info.host), "MySQL/MariaDB server to connect to.")
    ("port", po::value<int>(&info.port)->default_value(info.port), "TCP port of the server.")
    ("socket", po::value<std::string>(&info.socket), "Unix socket path (or Windows named pipe) to connect through instead of TCP.")
    ("database", po::value<std::string>(&info.database), "Default database (schema) to connect to.")
    ("user", po::value<std::string>(&info.user), "User to authenticate with.")
    ("password", po::value<std::string>(&info.password), "Password to authenticate with.")
    ("defaults-file", po::value<std::string>(&info.defaults_file), "my.cnf-style file whose [client] section supplies credentials, so passwords can be kept out of nsclient.ini.")
    ("plugin-dir", po::value<std::string>(&info.plugin_dir), "Directory the connector loads client auth plugins from (needed for MySQL 8's caching_sha2_password when the connector's default is wrong).")
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
