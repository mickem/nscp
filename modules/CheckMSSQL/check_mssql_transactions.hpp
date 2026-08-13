// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_transactions_command {

// Raw row: one open user transaction, with the owning session's longest-running
// request attached when it has one.
struct transaction_row {
  long long session_id = 0;
  std::string login;
  std::string database;
  std::string transaction_name;
  long long transaction_age = 0;
  long long request_age = -1;
  long long is_idle = 0;
  std::string command;
};

// One open user transaction as exposed to the filter engine.
struct transaction_info {
  long long session_id = 0;
  std::string login;
  std::string database;
  std::string transaction_name;
  long long transaction_age = 0;  // seconds since the transaction began
  long long request_age = -1;     // seconds the current request has been running, -1 = idle
  long long is_idle = 0;          // 1 = open transaction with no active request
  std::string command;            // command of the active request (empty when idle)

  long long get_session_id() const { return session_id; }
  std::string get_login() const { return login; }
  std::string get_database() const { return database; }
  std::string get_transaction_name() const { return transaction_name; }
  long long get_transaction_age() const { return transaction_age; }
  long long get_request_age() const { return request_age; }
  long long get_is_idle() const { return is_idle; }
  std::string get_command() const { return command; }

  std::string show() const { return std::to_string(session_id); }
};

typedef std::vector<transaction_info> transactions_type;

// Pure: one entry per session, keeping its oldest transaction. A session can
// hold more than one open transaction at a time (a distributed transaction
// enlisting several, for instance), and the oldest is the one pinning log
// truncation and version-store cleanup - reporting per session also keeps the
// session_id perfdata keys unique. Oldest first, as the most urgent.
transactions_type build_transactions(const std::vector<transaction_row> &rows);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_transactions_command
