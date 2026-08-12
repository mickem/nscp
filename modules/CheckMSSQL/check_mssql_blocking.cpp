// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_blocking.hpp"

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <unordered_map>
#include <unordered_set>

#include "mssql_filter_helpers.hpp"
#include "mssql_options.hpp"

namespace check_mssql_blocking_command {

namespace {

// One row per blocked request. The blocker's own request is LEFT JOINed to
// tell an actively working blocker from a sleeping one: a blocker with no
// active request is holding locks inside an open transaction while idle - the
// classic orphaned-transaction "the application is frozen" case.
const char *BLOCKING_SQL =
    "SELECT r.session_id, r.blocking_session_id,"
    " ISNULL(DB_NAME(r.database_id), '') AS database_name,"
    " ISNULL(s.login_name, '') AS login_name,"
    " ISNULL(bs.login_name, '') AS blocking_login,"
    " r.wait_time / 1000 AS wait_time,"
    " ISNULL(r.wait_type, '') AS wait_type,"
    " r.command,"
    " CASE WHEN br.session_id IS NULL THEN 1 ELSE 0 END AS blocker_idle"
    " FROM sys.dm_exec_requests r"
    " JOIN sys.dm_exec_sessions s ON s.session_id = r.session_id"
    " LEFT JOIN sys.dm_exec_sessions bs ON bs.session_id = r.blocking_session_id"
    " LEFT JOIN sys.dm_exec_requests br ON br.session_id = r.blocking_session_id"
    " WHERE r.blocking_session_id <> 0";

typedef blocking_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("database", &filter_obj::get_database, "Database the blocked request runs in (empty if unavailable)")
      .add_string_var("login", &filter_obj::get_login, "Login of the blocked session")
      .add_string_var("blocking_login", &filter_obj::get_blocking_login, "Login of the direct blocker")
      .add_string_var("wait_type", &filter_obj::get_wait_type, "Wait type of the blocked request, e.g. LCK_M_X")
      .add_string_var("command", &filter_obj::get_command, "Command the blocked request is executing, e.g. UPDATE");

  static const parsers::where::value_type type_age = parsers::where::type_custom_int_1;
  registry_.add_converter(type_age, &mssql_filter::parse_time<std::shared_ptr<filter_obj>>);
  registry_
      .add_int_var("wait_time", type_age, &filter_obj::get_wait_time,
                   "Seconds the request has been blocked (supports units, e.g. wait_time > 5m)")
      .add_int_perf("s", "", "_wait_time")
      .add_int_var("session_id", &filter_obj::get_session_id, "Session id of the blocked request")
      .no_perf()
      .add_int_var("blocking_session_id", &filter_obj::get_blocking_session_id, "Session id of the direct blocker")
      .no_perf()
      .add_int_var("root_blocker", &filter_obj::get_root_blocker, "Session id at the head of this blocking chain")
      .no_perf()
      .add_int_var("blocker_idle", &filter_obj::get_blocker_idle, "1 if the direct blocker is idle (holding locks with no active request)")
      .no_perf();
}

}  // namespace

blocking_type build_blocking(const std::vector<blocking_row> &rows) {
  std::unordered_map<long long, long long> blocker_of;
  for (const blocking_row &row : rows) blocker_of[row.session_id] = row.blocking_session_id;

  blocking_type result;
  for (const blocking_row &row : rows) {
    blocking_info info;
    info.session_id = row.session_id;
    info.blocking_session_id = row.blocking_session_id;
    info.database = row.database;
    info.login = row.login;
    info.blocking_login = row.blocking_login;
    info.wait_time = row.wait_time;
    info.wait_type = row.wait_type;
    info.command = row.command;
    info.blocker_idle = row.blocker_idle;

    long long root = row.blocking_session_id;
    std::unordered_set<long long> seen;
    seen.insert(row.session_id);
    while (blocker_of.count(root) && !seen.count(root)) {
      seen.insert(root);
      root = blocker_of[root];
    }
    info.root_blocker = root;
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
  // Momentary lock waits are normal; sustained ones are not. 30s of blocking
  // is already user-visible, 5 minutes means the application is frozen.
  filter_helper.add_options("wait_time > 30", "wait_time > 300", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} blocked sessions (${problem_list})",
                           "${database}/${login} blocked by session ${blocking_session_id} (${blocking_login}) for ${wait_time}s on ${wait_type}",
                           "${session_id}", "%(status): No blocked sessions", "%(status): %(count) blocked sessions, none over the thresholds");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(BLOCKING_SQL);
    std::vector<blocking_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      blocking_row row;
      row.session_id = res.get_int(i, "session_id");
      row.blocking_session_id = res.get_int(i, "blocking_session_id");
      row.database = res.get_string(i, "database_name");
      row.login = res.get_string(i, "login_name");
      row.blocking_login = res.get_string(i, "blocking_login");
      row.wait_time = res.get_int(i, "wait_time");
      row.wait_type = res.get_string(i, "wait_type");
      row.command = res.get_string(i, "command");
      row.blocker_idle = res.get_int(i, "blocker_idle") != 0;
      rows.push_back(row);
    }

    for (const blocking_info &blocked : build_blocking(rows)) {
      auto record = std::make_shared<filter_obj>(blocked);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_blocking_command
