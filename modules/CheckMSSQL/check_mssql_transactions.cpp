// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_transactions.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_filter_helpers.hpp"
#include "mssql_options.hpp"

namespace check_mssql_transactions_command {

namespace {

// One row per open user transaction. An old open transaction blocks log
// truncation (the log grows until the disk fills) and pins version-store
// cleanup (tempdb grows) long before anything visibly breaks. is_idle = 1
// (open transaction, no active request) is the classic leaked transaction: an
// application that crashed or forgot to COMMIT, which never resolves by
// itself. The check's own session is excluded - its autocommit transaction
// would otherwise show up in every result.
const char *TRANSACTIONS_SQL =
    "SELECT st.session_id,"
    " ISNULL(s.login_name, '') AS login_name,"
    " ISNULL(DB_NAME(s.database_id), '') AS database_name,"
    " ISNULL(at.name, '') AS transaction_name,"
    " DATEDIFF(second, at.transaction_begin_time, GETDATE()) AS transaction_age,"
    " ISNULL(DATEDIFF(second, r.start_time, GETDATE()), -1) AS request_age,"
    " CASE WHEN r.session_id IS NULL THEN 1 ELSE 0 END AS is_idle,"
    " ISNULL(r.command, '') AS command"
    " FROM sys.dm_tran_session_transactions st"
    " JOIN sys.dm_tran_active_transactions at ON at.transaction_id = st.transaction_id"
    " JOIN sys.dm_exec_sessions s ON s.session_id = st.session_id"
    // OUTER APPLY rather than a LEFT JOIN on session_id alone: a MARS connection
    // runs several requests on one session, and joining them all multiplied the
    // transaction row - two rows for one transaction, a doubled count and a
    // duplicated perfdata key. The longest-running request is the relevant one.
    " OUTER APPLY (SELECT TOP 1 req.session_id, req.start_time, req.command FROM sys.dm_exec_requests req"
    " WHERE req.session_id = st.session_id ORDER BY req.start_time) r"
    " WHERE st.is_user_transaction = 1 AND st.session_id <> @@SPID";

typedef transaction_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("login", &filter_obj::get_login, "Login that owns the transaction")
      .add_string_var("database", &filter_obj::get_database, "Database context of the session")
      .add_string_var("transaction_name", &filter_obj::get_transaction_name, "Transaction name, e.g. user_transaction or implicit_transaction")
      .add_string_var("command", &filter_obj::get_command, "Command of the active request (empty when the session is idle)");

  static const parsers::where::value_type type_age = parsers::where::type_custom_int_1;
  registry_.add_converter(type_age, &mssql_filter::parse_time<std::shared_ptr<filter_obj>>);
  registry_
      .add_int_var("transaction_age", type_age, &filter_obj::get_transaction_age,
                   "Seconds since the transaction began (supports units, e.g. transaction_age > 30m)")
      .add_int_perf("s", "", "_transaction_age")
      .add_int_var("request_age", type_age, &filter_obj::get_request_age,
                   "Seconds the current request has been executing, -1 = no active request (supports units)")
      .no_perf()
      .add_int_var("session_id", &filter_obj::get_session_id, "Session id owning the transaction")
      .no_perf()
      .add_int_var("is_idle", &filter_obj::get_is_idle, "1 if the transaction is open but the session has no active request (leaked transaction)")
      .no_perf();
}

}  // namespace

transactions_type build_transactions(const std::vector<transaction_row> &rows) {
  std::map<long long, const transaction_row *> oldest;
  for (const transaction_row &row : rows) {
    const auto it = oldest.find(row.session_id);
    if (it == oldest.end() || row.transaction_age > it->second->transaction_age) oldest[row.session_id] = &row;
  }

  transactions_type result;
  for (const auto &entry : oldest) {
    const transaction_row &row = *entry.second;
    transaction_info info;
    info.session_id = row.session_id;
    info.login = row.login;
    info.database = row.database;
    info.transaction_name = row.transaction_name;
    info.transaction_age = row.transaction_age;
    info.request_age = row.request_age;
    info.is_idle = row.is_idle;
    info.command = row.command;
    result.push_back(info);
  }
  // Oldest first, as the query's own ORDER BY used to deliver; session id breaks
  // ties so the output stays stable between runs.
  std::sort(result.begin(), result.end(), [](const transaction_info &a, const transaction_info &b) {
    return a.transaction_age != b.transaction_age ? a.transaction_age > b.transaction_age : a.session_id < b.session_id;
  });
  return result;
}

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;

  filter_type filter;
  // An idle open transaction is broken after minutes (nothing will ever
  // commit it); a working one gets half an hour before it warns. Both block
  // log truncation and version-store cleanup for as long as they live.
  filter_helper.add_options("transaction_age > 1800 or is_idle = 1 and transaction_age > 300", "transaction_age > 7200", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} open transactions (${problem_list})",
                           "session ${session_id} (${database}/${login}) open for ${transaction_age}s (idle: ${is_idle})", "${session_id}",
                           "%(status): No open transactions", "%(status): %(count) open transactions, none over the thresholds");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(TRANSACTIONS_SQL);
    std::vector<transaction_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      transaction_row row;
      row.session_id = res.get_int(i, "session_id");
      row.login = res.get_string(i, "login_name");
      row.database = res.get_string(i, "database_name");
      row.transaction_name = res.get_string(i, "transaction_name");
      row.transaction_age = res.get_int(i, "transaction_age");
      row.request_age = res.get_int(i, "request_age");
      row.is_idle = res.get_int(i, "is_idle");
      row.command = res.get_string(i, "command");
      rows.push_back(row);
    }

    for (const transaction_info &open : build_transactions(rows)) {
      auto record = std::make_shared<filter_obj>(open);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_transactions_command
