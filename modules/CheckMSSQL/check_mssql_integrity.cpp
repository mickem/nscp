// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_integrity.hpp"

#include <cstdio>
#include <memory>
#include <nscapi/macros.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_filter_helpers.hpp"
#include "mssql_options.hpp"

namespace check_mssql_integrity_command {

namespace {

// Databases with their suspect-page count. suspect_pages rows mean the engine
// has already *seen* corruption (823/824/825 errors); the msdb table keeps at
// most 1000 rows and is not pruned automatically, so restored/repaired pages
// (event_type 4/5/7) are excluded to avoid alerting on history. tempdb is
// excluded: CHECKDB does not apply to it. The server clock is returned in the
// same query so ages are computed against the server's own time, not the
// agent's (which may be in another timezone).
const char *INTEGRITY_SQL =
    "SELECT d.name, CONVERT(varchar(23), GETDATE(), 121) AS server_now, ISNULL(sp.cnt, 0) AS suspect_pages"
    " FROM sys.databases d"
    " LEFT JOIN (SELECT database_id, COUNT(*) AS cnt FROM msdb.dbo.suspect_pages WHERE event_type IN (1, 2, 3, 6) GROUP BY database_id) sp"
    " ON sp.database_id = d.database_id"
    " WHERE d.state = 0 AND d.name <> 'tempdb' ORDER BY d.name";

typedef integrity_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Database name");

  static const parsers::where::value_type type_age = parsers::where::type_custom_int_1;
  registry_.add_converter(type_age, &mssql_filter::parse_time<std::shared_ptr<filter_obj>>);
  registry_
      .add_int_var("checkdb_age", type_age, &filter_obj::get_checkdb_age,
                   "Seconds since the last successful DBCC CHECKDB, -1 = never checked, -2 = unknown/no access (supports units, e.g. checkdb_age > 14d)")
      .add_int_perf("s", "", "_checkdb_age")
      .add_int_var("suspect_pages", &filter_obj::get_suspect_pages,
                   "Pages in msdb.dbo.suspect_pages with unresolved 823/824/825 errors - any value above 0 means the engine has seen corruption")
      .add_int_perf("", "", "_suspect_pages");
}

// days-from-civil (Hinnant): pure calendar arithmetic, no timezone involved.
long long days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(d) - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return static_cast<long long>(era) * 146097LL + static_cast<long long>(doe) - 719468LL;
}

}  // namespace

long long parse_sql_datetime(const std::string &text) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  if (sscanf(text.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) != 6) return -1;
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return -1;
  return days_from_civil(y, mo, d) * 86400LL + h * 3600LL + mi * 60LL + s;
}

integrity_type build_integrity(const std::vector<integrity_row> &rows, const std::string &server_now) {
  const long long now = parse_sql_datetime(server_now);
  integrity_type result;
  for (const integrity_row &row : rows) {
    integrity_info info;
    info.name = row.name;
    info.suspect_pages = row.suspect_pages;
    if (row.last_checkdb.compare(0, 4, "1900") == 0) {
      info.checkdb_age = -1;  // the 1900-01-01 sentinel: never checked
    } else {
      // The sentinel parses to a negative (pre-1970) value, so it must be
      // recognized before this validity check.
      const long long last = row.last_checkdb.empty() ? -1 : parse_sql_datetime(row.last_checkdb);
      info.checkdb_age = (last < 0 || now < 0) ? -2 : now - last;
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
  // A backup of a corrupt database restores a corrupt database: suspect pages
  // page immediately, and a stale (or never-run) CHECKDB warns. -2 (no access
  // to DBCC DBINFO, which needs sysadmin) deliberately stays quiet.
  filter_helper.add_options("checkdb_age > 1209600 or checkdb_age = -1", "suspect_pages > 0", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} databases (${problem_list})", "${name}: checkdb age ${checkdb_age}s, ${suspect_pages} suspect pages",
                           "${name}", "%(status): No databases found", "%(status): All %(count) databases checked recently, no suspect pages");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(INTEGRITY_SQL);
    std::string server_now;
    std::vector<integrity_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      integrity_row row;
      row.name = res.get_string(i, "name");
      row.suspect_pages = res.get_int(i, "suspect_pages");
      server_now = res.get_string(i, "server_now");
      rows.push_back(row);
    }

    // dbi_dbccLastKnownGood only exists inside the boot page, so one DBCC
    // DBINFO per database. Read by position (ParentObject, Object, Field,
    // VALUE): the Field *values* are internal names but play safe about
    // localized headers, like the LOGSPACE parsing. DBCC DBINFO needs
    // sysadmin; a denied or failed call leaves last_checkdb empty = unknown
    // rather than failing the whole check.
    for (integrity_row &row : rows) {
      std::string quoted = row.name;
      for (std::size_t pos = 0; (pos = quoted.find('\'', pos)) != std::string::npos; pos += 2) quoted.replace(pos, 1, "''");
      try {
        const mssql_odbc::result info_res = session.execute("DBCC DBINFO(N'" + quoted + "') WITH TABLERESULTS, NO_INFOMSGS");
        if (info_res.columns.size() == 4) {
          for (std::size_t i = 0; i < info_res.rows.size(); i++) {
            if (info_res.get_string(i, 2) == "dbi_dbccLastKnownGood") {
              row.last_checkdb = info_res.get_string(i, 3);
              break;
            }
          }
        }
      } catch (const mssql_odbc::odbc_exception &e) {
        NSC_DEBUG_MSG("DBCC DBINFO failed for '" + row.name + "', checkdb_age will be -2: " + e.reason());
      }
    }

    for (const integrity_info &db : build_integrity(rows, server_now)) {
      auto record = std::make_shared<filter_obj>(db);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_integrity_command
