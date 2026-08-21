// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_databases_command {

// Raw per-database row from sys.databases.
struct database_row {
  long long database_id = 0;
  std::string name;
  std::string state;           // state_desc: ONLINE, RESTORING, RECOVERING, ...
  std::string recovery_model;  // recovery_model_desc: SIMPLE, FULL, BULK_LOGGED
  bool is_read_only = false;
};

// Raw per-database row from DBCC SQLPERF(LOGSPACE).
struct logspace_row {
  std::string name;
  long long used_pct = 0;
};

// One data or log file from sys.master_files, with its volume merged in from
// sys.dm_os_volume_stats. Sizes are bytes, not the 8KB pages the DMV reports.
struct file_row {
  long long database_id = 0;
  long long file_id = 0;
  int type = 0;                    // 0 = data (ROWS), 1 = log
  int data_space_id = 0;           // filegroup; every log file reports 0
  long long growth = 0;            // 0 = autogrowth disabled (pages or percent otherwise)
  long long size_bytes = 0;        // current size
  long long max_size_bytes = 0;    // -1 = unlimited, 0 = cannot grow, otherwise the cap
  std::string volume;              // volume_mount_point; empty = unknown
  long long available_bytes = -1;  // free space on that volume, -1 = unknown
};

// Growth headroom for one database, in bytes; -1 = unknown.
struct headroom_info {
  long long data = -1;
  long long log = -1;
};

// One database as exposed to the filter engine.
struct database_info {
  std::string name;
  std::string state;
  std::string recovery_model;
  bool is_read_only = false;
  long long data_size = 0;
  long long log_size = 0;
  long long log_used_pct = -1;   // -1 = unknown (LOGSPACE unavailable or database missing from it)
  long long data_headroom = -1;  // smallest remaining growth room among the data files in bytes, -1 = unknown
  long long log_headroom = -1;   // same for the log files

  std::string get_name() const { return name; }
  std::string get_state() const { return state; }
  std::string get_recovery_model() const { return recovery_model; }
  long long get_is_read_only() const { return is_read_only ? 1 : 0; }
  long long get_data_size() const { return data_size; }
  long long get_log_size() const { return log_size; }
  long long get_log_used_pct() const { return log_used_pct; }
  long long get_data_headroom() const { return data_headroom; }
  long long get_log_headroom() const { return log_headroom; }

  std::string show() const { return name; }
};

typedef std::vector<database_info> databases_type;

// Pure: growth headroom per database, keyed by database_id, in bytes; -1 when it
// cannot be determined. Rolled up in three steps, because each level has a
// different reason:
//
//  - Per volume, the files on it share its free space, so together they can add
//    at most that much - counting it once per file would report a default
//    8-file tempdb as eight times the disk. They are also held to their own
//    max_size caps, so the volume's contribution is the smaller of the two. A
//    log file with unlimited growth still carries the engine's 2TB cap, which
//    is why the cap alone is not headroom on a nearly full volume.
//  - Per filegroup, the volumes are summed: proportional fill keeps allocating
//    in sibling files until every file in the group is full, so one fixed-size
//    file does not pin the group, and files on different volumes really do add
//    up. Files that cannot grow at all contribute nothing. Log files all report
//    data_space_id 0, so they roll up as one group.
//  - Per database, the most constrained filegroup wins: a full filegroup fails
//    writes to its own objects however much room another filegroup has.
//
// A file with no volume information makes its filegroup unknown, and an unknown
// filegroup makes the database unknown.
std::map<long long, headroom_info> compute_headroom(const std::vector<file_row> &files);

// Pure: assemble the filter objects from the three result sets. File sizes and
// headroom are merged by database_id (immune to a rename between queries);
// LOGSPACE reports names only, so log_used_pct merges by name. Databases absent
// from `logspace` keep log_used_pct = -1 and databases with no usable file rows
// keep data/log_headroom = -1.
databases_type build_databases(const std::vector<database_row> &databases, const std::vector<logspace_row> &logspace, const std::vector<file_row> &files);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_databases_command
