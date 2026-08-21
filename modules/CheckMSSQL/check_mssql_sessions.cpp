// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_sessions.hpp"

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_filter_helpers.hpp"
#include "mssql_options.hpp"

namespace check_mssql_sessions_command {

namespace {

// One row per (database, login) pair over the user sessions. Connections are
// counted per session with OUTER APPLY: joining sys.dm_exec_connections
// directly would multiply the session rows for MARS sessions and break the
// COUNT(*). max_idle only considers sleeping/dormant sessions (the same
// statuses the idle count uses): a running session's last_request_end_time
// describes its *previous* request, and counting it would flag a session that
// is busy, not leaked. last_request_end_time is NULL (or the 1900-01-01 epoch
// default) for sessions that never completed a request; both would break or
// skew the idle age (DATEDIFF overflows on the epoch), so they map to
// NULL = unknown.
const char *SESSIONS_SQL =
    "SELECT ISNULL(DB_NAME(s.database_id), '') AS database_name,"
    " ISNULL(s.login_name, '') AS login_name,"
    " COUNT(*) AS sessions,"
    " SUM(CASE WHEN s.status = 'running' THEN 1 ELSE 0 END) AS running,"
    " SUM(CASE WHEN s.status IN ('sleeping', 'dormant') THEN 1 ELSE 0 END) AS idle,"
    " ISNULL(SUM(c.conns), 0) AS connections,"
    " MAX(CASE WHEN s.status IN ('sleeping', 'dormant') AND s.last_request_end_time >= '2000-01-01'"
    " THEN DATEDIFF(second, s.last_request_end_time, GETDATE()) END) AS max_idle"
    " FROM sys.dm_exec_sessions s"
    " OUTER APPLY (SELECT COUNT(*) AS conns FROM sys.dm_exec_connections c WHERE c.session_id = s.session_id) c"
    " WHERE s.is_user_process = 1"
    " GROUP BY DB_NAME(s.database_id), s.login_name";

typedef session_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("database", &filter_obj::get_database, "Database the sessions are connected to (empty if unavailable)")
      .add_string_var("login", &filter_obj::get_login, "Login name the sessions authenticated as");

  static const parsers::where::value_type type_age = parsers::where::type_custom_int_1;
  registry_.add_converter(type_age, &mssql_filter::parse_time<std::shared_ptr<filter_obj>>);
  registry_.add_int_var("sessions", &filter_obj::get_sessions, "Number of sessions for this database/login pair")
      .add_int_perf("", "", "_sessions")
      .add_int_var("running", &filter_obj::get_running, "Sessions currently executing a request")
      .add_int_perf("", "", "_running")
      .add_int_var("idle", &filter_obj::get_idle, "Sessions that are sleeping or dormant")
      .no_perf()
      .add_int_var("connections", &filter_obj::get_connections, "Number of physical connections for this database/login pair")
      .add_int_perf("", "", "_connections")
      .add_int_var("max_idle", type_age, &filter_obj::get_max_idle,
                   "Seconds since the most idle sleeping/dormant session last completed a request (running sessions are excluded), -1 = unknown (supports "
                   "units, e.g. max_idle > 2h)")
      .add_int_perf("s", "", "_max_idle");
}

}  // namespace

sessions_type build_sessions(const std::vector<session_row> &rows) {
  sessions_type result;
  for (const session_row &row : rows) {
    session_info info;
    info.database = row.database;
    info.login = row.login;
    info.sessions = row.sessions;
    info.running = row.running;
    info.idle = row.idle;
    info.connections = row.connections;
    info.max_idle = row.has_max_idle ? row.max_idle : -1;
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
  // No default thresholds: healthy session counts are workload-specific, so
  // the check is informational until the user thresholds sessions/max_idle.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${database}/${login}: ${sessions} sessions (${running} running)", "${database}/${login}",
                           "%(status): No user sessions found", "");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(SESSIONS_SQL);
    std::vector<session_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      session_row row;
      row.database = res.get_string(i, "database_name");
      row.login = res.get_string(i, "login_name");
      row.sessions = res.get_int(i, "sessions");
      row.running = res.get_int(i, "running");
      row.idle = res.get_int(i, "idle");
      row.connections = res.get_int(i, "connections");
      row.has_max_idle = !res.is_null(i, "max_idle");
      if (row.has_max_idle) row.max_idle = res.get_int(i, "max_idle");
      rows.push_back(row);
    }

    for (const session_info &group : build_sessions(rows)) {
      auto record = std::make_shared<filter_obj>(group);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_sessions_command
