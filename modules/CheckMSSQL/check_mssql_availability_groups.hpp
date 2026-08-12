// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_availability_groups_command {

// Raw row from the AG catalog joined with the HADR state DMVs: one row per
// (group, replica, database), or per (group, replica) when no database-level
// state is visible from this instance.
struct replica_row {
  std::string group;
  std::string replica;
  std::string role;             // PRIMARY, SECONDARY or RESOLVING
  std::string connected_state;  // CONNECTED / DISCONNECTED
  std::string replica_health;   // HEALTHY, PARTIALLY_HEALTHY or NOT_HEALTHY
  bool is_local = false;
  std::string database;         // empty for replica-only rows
  std::string sync_state;       // SYNCHRONIZED, SYNCHRONIZING, NOT SYNCHRONIZING, ...
  std::string db_health;        // empty for replica-only rows
  long long redo_queue = 0;      // bytes
  long long log_send_queue = 0;  // bytes
  bool is_suspended = false;
};

// One AG object (replica or replica database) as exposed to the filter engine.
struct replica_info {
  std::string name;  // group/replica or group/replica/database
  std::string group;
  std::string replica;
  std::string role;
  std::string connected_state;
  std::string replica_health;
  std::string health;  // db_health for database rows, replica_health otherwise
  long long is_local = 0;
  std::string database;
  std::string sync_state;
  std::string db_health;
  long long redo_queue = 0;
  long long log_send_queue = 0;
  long long is_suspended = 0;

  std::string get_name() const { return name; }
  std::string get_group() const { return group; }
  std::string get_replica() const { return replica; }
  std::string get_role() const { return role; }
  std::string get_connected_state() const { return connected_state; }
  std::string get_replica_health() const { return replica_health; }
  std::string get_health() const { return health; }
  long long get_is_local() const { return is_local; }
  std::string get_database() const { return database; }
  std::string get_sync_state() const { return sync_state; }
  std::string get_db_health() const { return db_health; }
  long long get_redo_queue() const { return redo_queue; }
  long long get_log_send_queue() const { return log_send_queue; }
  long long get_is_suspended() const { return is_suspended; }

  std::string show() const { return name; }
};

typedef std::vector<replica_info> replicas_type;

// Pure: map the raw rows into filter objects, composing the display name and
// picking the effective health (database health when present, else replica).
replicas_type build_replicas(const std::vector<replica_row> &rows);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_availability_groups_command
