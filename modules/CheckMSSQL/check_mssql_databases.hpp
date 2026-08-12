// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_databases_command {

// Raw per-database row from sys.databases + sys.master_files.
struct database_row {
  std::string name;
  std::string state;           // state_desc: ONLINE, RESTORING, RECOVERING, ...
  std::string recovery_model;  // recovery_model_desc: SIMPLE, FULL, BULK_LOGGED
  bool is_read_only = false;
  long long data_bytes = 0;
  long long log_bytes = 0;
};

// Raw per-database row from DBCC SQLPERF(LOGSPACE).
struct logspace_row {
  std::string name;
  long long used_pct = 0;
};

// Raw growth headroom per database and file type (0 = data, 1 = log): the
// smallest remaining growth room among the files of that type, in bytes.
struct headroom_row {
  std::string name;
  int type = 0;
  long long headroom = 0;
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

// Pure: join the sys.databases rows with the LOGSPACE and headroom rows by
// database name. Databases absent from `logspace` keep log_used_pct = -1;
// databases absent from `headroom` keep data/log_headroom = -1. Negative
// headroom (a file shrunk below its former max) is clamped to 0.
databases_type build_databases(const std::vector<database_row> &databases, const std::vector<logspace_row> &logspace,
                               const std::vector<headroom_row> &headroom);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_databases_command
