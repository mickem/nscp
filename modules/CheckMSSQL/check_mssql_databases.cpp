// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_databases.hpp"

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_options.hpp"

namespace check_mssql_databases_command {

namespace {

const char *DATABASES_SQL =
    "SELECT d.name, d.state_desc AS state, d.recovery_model_desc AS recovery_model, d.is_read_only,"
    " SUM(CASE WHEN f.type = 0 THEN CAST(f.size AS bigint) * 8192 ELSE 0 END) AS data_bytes,"
    " SUM(CASE WHEN f.type = 1 THEN CAST(f.size AS bigint) * 8192 ELSE 0 END) AS log_bytes"
    " FROM sys.databases d"
    " LEFT JOIN sys.master_files f ON f.database_id = d.database_id"
    " GROUP BY d.name, d.state_desc, d.recovery_model_desc, d.is_read_only";

typedef database_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Database name")
      .add_string_var("state", &filter_obj::get_state, "Database state: ONLINE, RESTORING, RECOVERING, RECOVERY_PENDING, SUSPECT, EMERGENCY or OFFLINE")
      .add_string_var("recovery_model", &filter_obj::get_recovery_model, "Recovery model: SIMPLE, FULL or BULK_LOGGED");

  registry_
      .add_int_var("data_size", parsers::where::type_size, &filter_obj::get_data_size,
                   "Total size of the data files in bytes (supports units, e.g. data_size > 10G)")
      .add_int_perf("B", "", "_data")
      .add_int_var("log_size", parsers::where::type_size, &filter_obj::get_log_size,
                   "Total size of the log files in bytes (supports units, e.g. log_size > 1G)")
      .add_int_perf("B", "", "_log")
      .add_int_var("log_used_pct", &filter_obj::get_log_used_pct, "Percentage of the log in use (-1 if unavailable)")
      .add_int_perf("%", "", "_log_used_pct")
      .add_int_var("is_read_only", &filter_obj::get_is_read_only, "1 if the database is read-only")
      .no_perf();
}

}  // namespace

databases_type build_databases(const std::vector<database_row> &databases, const std::vector<logspace_row> &logspace) {
  databases_type result;
  for (const database_row &row : databases) {
    database_info info;
    info.name = row.name;
    info.state = row.state;
    info.recovery_model = row.recovery_model;
    info.is_read_only = row.is_read_only;
    info.data_size = row.data_bytes;
    info.log_size = row.log_bytes;
    for (const logspace_row &ls : logspace) {
      if (ls.name == row.name) {
        info.log_used_pct = ls.used_pct;
        break;
      }
    }
    result.push_back(info);
  }
  return result;
}

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;

  filter_type filter;
  // OFFLINE only warns since taking a database offline can be intentional;
  // filter it away (filter=state != 'OFFLINE') if that is routine on this host.
  filter_helper.add_options("state = 'RESTORING' or state = 'RECOVERING' or state = 'OFFLINE'",
                            "state = 'SUSPECT' or state = 'EMERGENCY' or state = 'RECOVERY_PENDING'", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} databases (${problem_list})", "${name}: ${state}", "${name}", "%(status): No databases found",
                           "%(status): All %(count) databases are ONLINE");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(DATABASES_SQL);
    std::vector<database_row> databases;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      database_row row;
      row.name = res.get_string(i, "name");
      row.state = res.get_string(i, "state");
      row.recovery_model = res.get_string(i, "recovery_model");
      row.is_read_only = res.get_int(i, "is_read_only") != 0;
      row.data_bytes = res.get_int(i, "data_bytes");
      row.log_bytes = res.get_int(i, "log_bytes");
      databases.push_back(row);
    }

    // Log usage is nice-to-have: if DBCC SQLPERF is denied or fails, keep the
    // check working and report log_used_pct = -1.
    std::vector<logspace_row> logspace;
    try {
      const mssql_odbc::result ls = session.execute("DBCC SQLPERF(LOGSPACE)");
      for (std::size_t i = 0; i < ls.rows.size(); i++) {
        logspace_row row;
        row.name = ls.get_string(i, "Database Name");
        row.used_pct = ls.get_int(i, "Log Space Used (%)");
        logspace.push_back(row);
      }
    } catch (const mssql_odbc::odbc_exception &) {
    }

    for (const database_info &db : build_databases(databases, logspace)) {
      auto record = std::make_shared<filter_obj>(db);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_databases_command
