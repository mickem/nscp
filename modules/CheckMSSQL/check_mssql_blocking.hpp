// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_blocking_command {

// Raw row from sys.dm_exec_requests: one row per currently blocked request.
struct blocking_row {
  long long session_id = 0;
  long long blocking_session_id = 0;
  std::string database;
  std::string login;
  std::string blocking_login;
  long long wait_time = 0;  // seconds
  std::string wait_type;
  std::string command;
  bool blocker_idle = false;  // the direct blocker has no active request (sleeping while holding locks)
};

// One blocked session as exposed to the filter engine.
struct blocking_info {
  long long session_id = 0;
  long long blocking_session_id = 0;
  long long root_blocker = 0;  // session at the head of this blocking chain
  std::string database;
  std::string login;
  std::string blocking_login;
  long long wait_time = 0;  // seconds
  std::string wait_type;
  std::string command;
  bool blocker_idle = false;

  long long get_session_id() const { return session_id; }
  long long get_blocking_session_id() const { return blocking_session_id; }
  long long get_root_blocker() const { return root_blocker; }
  std::string get_database() const { return database; }
  std::string get_login() const { return login; }
  std::string get_blocking_login() const { return blocking_login; }
  long long get_wait_time() const { return wait_time; }
  std::string get_wait_type() const { return wait_type; }
  std::string get_command() const { return command; }
  long long get_blocker_idle() const { return blocker_idle ? 1 : 0; }

  std::string show() const { return std::to_string(session_id); }
};

typedef std::vector<blocking_info> blocking_type;

// Pure: map the blocked-request rows into filter objects and resolve each
// chain to its root blocker (the session everyone is ultimately waiting on).
// Walks blocked -> blocker until the blocker is not itself blocked; a cycle
// (deadlock in flight) terminates at the first revisited session.
blocking_type build_blocking(const std::vector<blocking_row> &rows);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_blocking_command
