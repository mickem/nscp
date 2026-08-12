// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_sessions_command {

// Raw aggregated row from sys.dm_exec_sessions / sys.dm_exec_connections:
// one row per (database, login) pair.
struct session_row {
  std::string database;
  std::string login;
  long long sessions = 0;
  long long running = 0;
  long long idle = 0;
  long long connections = 0;
  bool has_max_idle = false;  // false when the group has no sleeping/dormant session with a completed request
  long long max_idle = 0;
};

// One (database, login) group as exposed to the filter engine.
struct session_info {
  std::string database;
  std::string login;
  long long sessions = 0;
  long long running = 0;
  long long idle = 0;
  long long connections = 0;
  long long max_idle = -1;  // seconds since the most idle sleeping/dormant session's last request ended, -1 = unknown

  std::string get_database() const { return database; }
  std::string get_login() const { return login; }
  long long get_sessions() const { return sessions; }
  long long get_running() const { return running; }
  long long get_idle() const { return idle; }
  long long get_connections() const { return connections; }
  long long get_max_idle() const { return max_idle; }

  std::string show() const { return database.empty() ? login : database + "/" + login; }
};

typedef std::vector<session_info> sessions_type;

// Pure: map the aggregated rows into filter objects; groups with no idle
// session that has completed a request keep max_idle = -1.
sessions_type build_sessions(const std::vector<session_row> &rows);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_sessions_command
