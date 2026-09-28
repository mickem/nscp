// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_tempdb_command {

// Raw aggregated tempdb space usage (bytes) plus the free space on the most
// constrained volume holding a tempdb data file (-1 = unavailable).
struct tempdb_row {
  long long size = 0;
  long long free = 0;
  long long version_store = 0;
  long long user_objects = 0;
  long long internal_objects = 0;
  long long volume_free = -1;
};

// tempdb as exposed to the filter engine (one row).
struct tempdb_info {
  long long size = 0;              // allocated data-file bytes
  long long free = 0;              // unallocated bytes within the files
  long long used = 0;              // size - free
  long long used_pct = 0;          // percent of the allocation in use
  long long version_store = 0;     // bytes held by the version store
  long long user_objects = 0;      // bytes held by user objects (temp tables, table variables)
  long long internal_objects = 0;  // bytes held by internal objects (spills, work tables)
  long long volume_free = -1;      // free bytes on the most constrained tempdb data volume, -1 = unknown

  long long get_size() const { return size; }
  long long get_free() const { return free; }
  long long get_used() const { return used; }
  long long get_used_pct() const { return used_pct; }
  long long get_version_store() const { return version_store; }
  long long get_user_objects() const { return user_objects; }
  long long get_internal_objects() const { return internal_objects; }
  long long get_volume_free() const { return volume_free; }

  std::string show() const { return "tempdb"; }
};

// Pure: derive used/used_pct from the raw row.
tempdb_info build_tempdb(const tempdb_row &row);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_tempdb_command
