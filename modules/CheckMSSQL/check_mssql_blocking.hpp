// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_blocking_command {

// Raw row from sys.dm_exec_requests: one row per request in flight, blocked or
// not. The unblocked rows are needed too - whether the blocker has a request of
// its own is what separates a working blocker from one sitting idle in an open
// transaction.
struct request_row {
  long long session_id = 0;
  long long blocking_session_id = 0;
  std::string database;
  long long wait_time = 0;  // seconds
  std::string wait_type;
  std::string command;
};

// Raw row from sys.dm_exec_sessions.
struct session_row {
  long long session_id = 0;
  std::string login;
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

// Pure: pick the blocked requests out of every request in flight, attach logins
// from the session rows, and resolve each chain to its root blocker (the session
// everyone is ultimately waiting on). Walks blocked -> blocker until the blocker
// is not itself blocked; a cycle (a deadlock in flight) terminates at the first
// revisited session.
//
// A request counts as blocked only when blocking_session_id names another
// session: 0 means nothing blocks it, the documented negative values (-2
// orphaned distributed transaction, -3 deferred recovery, -4 latch state
// undetermined) name no session at all, and a value equal to session_id is a
// parallel query waiting on its own threads (CXPACKET/CXCONSUMER), not a
// conflict between two sessions. One blocked entry is reported per session,
// keeping its longest-waiting request, so that a session with several requests
// blocked at once stays a single row with a unique perfdata key.
blocking_type build_blocking(const std::vector<request_row> &requests, const std::vector<session_row> &sessions);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_blocking_command
