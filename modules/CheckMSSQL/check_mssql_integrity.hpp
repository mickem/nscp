// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_integrity_command {

// Raw per-database integrity inputs.
struct integrity_row {
  std::string name;
  long long suspect_pages = 0;
  std::string last_checkdb;  // dbi_dbccLastKnownGood as text; empty = DBCC DBINFO unavailable
};

// One database as exposed to the filter engine.
struct integrity_info {
  std::string name;
  long long suspect_pages = 0;
  long long checkdb_age = -2;  // seconds since last successful CHECKDB; -1 = never, -2 = unknown

  std::string get_name() const { return name; }
  long long get_suspect_pages() const { return suspect_pages; }
  long long get_checkdb_age() const { return checkdb_age; }

  std::string show() const { return name; }
};

typedef std::vector<integrity_info> integrity_type;

// Pure: parse "YYYY-MM-DD hh:mm:ss[.mmm]" into seconds since the civil epoch
// (no timezone conversion - callers diff two timestamps from the same clock).
// Returns -1 when the string does not parse.
long long parse_sql_datetime(const std::string &text);

// Pure: fold the raw rows into filter objects. Ages are computed against
// server_now (the server's own clock, same format) so an agent in a different
// timezone does not skew them; the 1900-01-01 sentinel means never checked.
integrity_type build_integrity(const std::vector<integrity_row> &rows, const std::string &server_now);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_integrity_command
