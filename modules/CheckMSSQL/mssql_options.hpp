// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <string>

#include "odbc_query.hpp"

namespace mssql_options {

// Shared connection options for all CheckMSSQL commands. `info` arrives
// pre-populated with the module's /settings/mssql defaults, so options only
// override what the user passes explicitly.
inline void add_connection_options(boost::program_options::options_description &desc, mssql_odbc::connection_info &info) {
  namespace po = boost::program_options;
  // clang-format off
  desc.add_options()
    ("server", po::value<std::string>(&info.server)->default_value(info.server), "SQL Server to connect to: host, host\\INSTANCE or host,port.")
    ("database", po::value<std::string>(&info.database), "Database (initial catalog) to connect to (default: the login's default database).")
    ("user", po::value<std::string>(&info.user), "SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication.")
    ("password", po::value<std::string>(&info.password), "Password for the SQL login.")
    ("driver", po::value<std::string>(&info.driver), "ODBC driver to use (default: newest installed SQL Server driver).")
    ("connection-string", po::value<std::string>(&info.raw_connection_string), "Raw ODBC connection string; overrides all other connection options.")
    ("timeout", po::value<int>(&info.login_timeout)->default_value(info.login_timeout), "Connection (login) timeout in seconds.")
    ("query-timeout", po::value<int>(&info.query_timeout)->default_value(info.query_timeout), "Query timeout in seconds.")
    ("trust-cert", po::value<bool>(&info.trust_server_cert)->implicit_value(true)->default_value(info.trust_server_cert), "Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).")
    ("encrypt", po::value<std::string>(&info.encrypt), "Force connection encryption on or off: yes or no (modern ODBC drivers only).")
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

// Connect and run `body(session)` with the module's stable error contract:
// connect failures become "Failed to connect to SQL Server '<server>': ...",
// query failures "Query failed: ..." and anything else raised while running the
// check "Check failed: ..." — all UNKNOWN.
template <class TBody>
void with_session(const mssql_odbc::connection_info &info, PB::Commands::QueryResponseMessage::Response *response, TBody body) {
  try {
    mssql_odbc::session session(info);
    try {
      session.connect();
    } catch (const mssql_odbc::odbc_exception &e) {
      return fail(response, "Failed to connect to SQL Server '" + info.server + "': " + e.reason());
    }
    try {
      body(session);
    } catch (const mssql_odbc::odbc_exception &e) {
      return fail(response, "Query failed: " + e.reason());
    } catch (const std::exception &e) {
      // The connection succeeded, so this is a rendering/threshold failure, not
      // a connect failure; label it accordingly instead of misdirecting the
      // operator to the network.
      return fail(response, std::string("Check failed: ") + e.what());
    }
  } catch (const std::exception &e) {
    // Only session construction (handle allocation) can reach here.
    return fail(response, std::string("Failed to connect to SQL Server: ") + e.what());
  }
}

}  // namespace mssql_options
