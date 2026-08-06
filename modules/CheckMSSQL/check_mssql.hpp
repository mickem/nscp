// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>

#include "odbc_query.hpp"

namespace check_mssql_command {

// One connected SQL Server instance (SERVERPROPERTY + sys.dm_os_sys_info).
struct server_info {
  std::string server_name;
  std::string version;        // ProductVersion, e.g. 16.0.1000.6
  std::string product_level;  // RTM / SPn / CUn
  std::string edition;        // e.g. Express Edition (64-bit)
  long long uptime = 0;       // seconds since sqlserver_start_time

  std::string get_server_name() const { return server_name; }
  std::string get_version() const { return version; }
  std::string get_product_level() const { return product_level; }
  std::string get_edition() const { return edition; }
  long long get_uptime() const { return uptime; }

  std::string show() const { return server_name; }
};

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_command
