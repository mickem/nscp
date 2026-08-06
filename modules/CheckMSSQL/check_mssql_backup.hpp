// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_backup_command {

// Raw per-database row: ages are seconds, or negative when the backup type has
// never been taken (NULL from SQL).
struct backup_row {
  std::string name;
  std::string recovery_model;
  bool has_full = false;
  bool has_diff = false;
  bool has_log = false;
  long long full_age = 0;
  long long diff_age = 0;
  long long log_age = 0;
};

// One database as exposed to the filter engine; -1 = never backed up.
struct backup_info {
  std::string name;
  std::string recovery_model;
  long long full_age = -1;
  long long diff_age = -1;
  long long log_age = -1;

  std::string get_name() const { return name; }
  std::string get_recovery_model() const { return recovery_model; }
  long long get_full_age() const { return full_age; }
  long long get_diff_age() const { return diff_age; }
  long long get_log_age() const { return log_age; }

  std::string show() const { return name; }
};

typedef std::vector<backup_info> backups_type;

// Pure: normalize raw rows, mapping missing backups to -1.
backups_type build_backups(const std::vector<backup_row> &rows);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_backup_command
