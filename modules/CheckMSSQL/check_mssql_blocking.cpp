// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_blocking.hpp"

#include <map>
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

// Two flat selects instead of one four-way join: which requests count as
// blocked, which of a session's requests to report, and whether the blocker is
// idle are all decisions, and build_blocking() makes them where they can be
// unit-tested. Joining the blocker's request set in SQL also multiplied the
// blocked row when the blocker had several requests in flight (a MARS
// connection), inflating both the count and the perfdata.
//
// Every request is fetched, not just the blocked ones: a blocker with no
// request of its own is holding locks inside an open transaction while idle -
// the classic orphaned-transaction "the application is frozen" case - and that
// is only visible if the unblocked rows come too.
const char *REQUESTS_SQL =
    "SELECT r.session_id, r.blocking_session_id,"
    " ISNULL(DB_NAME(r.database_id), '') AS database_name,"
    " r.wait_time / 1000 AS wait_time,"
    " ISNULL(r.wait_type, '') AS wait_type,"
    " r.command"
    " FROM sys.dm_exec_requests r";

const char *SESSIONS_SQL = "SELECT s.session_id, ISNULL(s.login_name, '') AS login_name FROM sys.dm_exec_sessions s";

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

bool is_blocked_by_another_session(const request_row &row) { return row.blocking_session_id > 0 && row.blocking_session_id != row.session_id; }

}  // namespace

blocking_type build_blocking(const std::vector<request_row> &requests, const std::vector<session_row> &sessions) {
  std::unordered_map<long long, std::string> login_of;
  for (const session_row &row : sessions) login_of[row.session_id] = row.login;
  std::unordered_set<long long> has_request;
  for (const request_row &row : requests) has_request.insert(row.session_id);

  // Keyed by session, so a MARS session blocked on several requests at once
  // stays one row; the longest wait is the one worth reporting. std::map also
  // makes the output order deterministic.
  std::map<long long, const request_row *> blocked;
  for (const request_row &row : requests) {
    if (!is_blocked_by_another_session(row)) continue;
    const auto it = blocked.find(row.session_id);
    if (it == blocked.end() || row.wait_time > it->second->wait_time) blocked[row.session_id] = &row;
  }

  std::unordered_map<long long, long long> blocker_of;
  for (const auto &entry : blocked) blocker_of[entry.first] = entry.second->blocking_session_id;

  blocking_type result;
  for (const auto &entry : blocked) {
    const request_row &row = *entry.second;
    blocking_info info;
    info.session_id = row.session_id;
    info.blocking_session_id = row.blocking_session_id;
    info.database = row.database;
    info.wait_time = row.wait_time;
    info.wait_type = row.wait_type;
    info.command = row.command;
    const auto login = login_of.find(row.session_id);
    if (login != login_of.end()) info.login = login->second;
    const auto blocking_login = login_of.find(row.blocking_session_id);
    if (blocking_login != login_of.end()) info.blocking_login = blocking_login->second;
    info.blocker_idle = has_request.count(row.blocking_session_id) == 0;

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
    const mssql_odbc::result res = session.execute(REQUESTS_SQL);
    std::vector<request_row> requests;
    bool any_blocked = false;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      request_row row;
      row.session_id = res.get_int(i, "session_id");
      row.blocking_session_id = res.get_int(i, "blocking_session_id");
      row.database = res.get_string(i, "database_name");
      row.wait_time = res.get_int(i, "wait_time");
      row.wait_type = res.get_string(i, "wait_type");
      row.command = res.get_string(i, "command");
      if (row.blocking_session_id > 0) any_blocked = true;
      requests.push_back(row);
    }

    // Logins are only ever needed for a blocked session or its blocker, and
    // dm_exec_sessions runs into the thousands on a host with connection pools.
    // The guard is deliberately looser than build_blocking's own rule - a
    // superset, so it can over-fetch but never miss a login.
    std::vector<session_row> sessions;
    if (any_blocked) {
      const mssql_odbc::result ses = session.execute(SESSIONS_SQL);
      for (std::size_t i = 0; i < ses.rows.size(); i++) {
        session_row row;
        row.session_id = ses.get_int(i, "session_id");
        row.login = ses.get_string(i, "login_name");
        sessions.push_back(row);
      }
    }

    for (const blocking_info &blocked : build_blocking(requests, sessions)) {
      auto record = std::make_shared<filter_obj>(blocked);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_blocking_command
