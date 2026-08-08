// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_backup.hpp"

#include <boost/program_options.hpp>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

#include "mssql_filter_helpers.hpp"
#include "mssql_options.hpp"

namespace po = boost::program_options;

namespace check_mssql_backup_command {

// tempdb (database_id 2) is never backed up, so it is excluded up front.
//
// The copy-only/snapshot predicates belong in the JOIN's ON clause, never in
// WHERE: in WHERE they would turn the LEFT JOIN into an inner join and drop
// every database with no matching backup — exactly the never-backed-up ones
// this check exists to find.
std::string build_backup_sql(const bool include_copy_only, const bool include_snapshot) {
  std::string join_filter;
  if (!include_copy_only) join_filter += " AND b.is_copy_only = 0";
  if (!include_snapshot) join_filter += " AND b.is_snapshot = 0";
  return "SELECT d.name, d.recovery_model_desc AS recovery_model,"
         " DATEDIFF(second, MAX(CASE WHEN b.type = 'D' THEN b.backup_finish_date END), GETDATE()) AS full_age,"
         " DATEDIFF(second, MAX(CASE WHEN b.type = 'I' THEN b.backup_finish_date END), GETDATE()) AS diff_age,"
         " DATEDIFF(second, MAX(CASE WHEN b.type = 'L' THEN b.backup_finish_date END), GETDATE()) AS log_age"
         " FROM sys.databases d"
         " LEFT JOIN msdb.dbo.backupset b ON b.database_name = d.name" +
         join_filter +
         " WHERE d.database_id <> 2"
         " GROUP BY d.name, d.recovery_model_desc";
}

namespace {

typedef backup_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Database name")
      .add_string_var("recovery_model", &filter_obj::get_recovery_model, "Recovery model: SIMPLE, FULL or BULK_LOGGED");

  static const parsers::where::value_type type_age = parsers::where::type_custom_int_1;
  registry_.add_int_var("full_age", type_age, &filter_obj::get_full_age, "Seconds since the last full backup, -1 = never (supports units, e.g. full_age > 7d)")
      .add_int_perf("s", "", "_full_age")
      .add_int_var("diff_age", type_age, &filter_obj::get_diff_age, "Seconds since the last differential backup, -1 = never (supports units)")
      .add_int_perf("s", "", "_diff_age")
      .add_int_var("log_age", type_age, &filter_obj::get_log_age, "Seconds since the last log backup, -1 = never (supports units, e.g. log_age > 1h)")
      .add_int_perf("s", "", "_log_age");
  registry_.add_converter(type_age, &mssql_filter::parse_time<std::shared_ptr<filter_obj>>);
}

}  // namespace

backups_type build_backups(const std::vector<backup_row> &rows) {
  backups_type result;
  for (const backup_row &row : rows) {
    backup_info info;
    info.name = row.name;
    info.recovery_model = row.recovery_model;
    info.full_age = row.has_full ? row.full_age : -1;
    info.diff_age = row.has_diff ? row.diff_age : -1;
    info.log_age = row.has_log ? row.log_age : -1;
    result.push_back(info);
  }
  return result;
}

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;
  bool include_copy_only = false, include_snapshot = false;

  filter_type filter;
  // Default policy: a full backup must exist and be younger than 7 days
  // (warn at 3 days). Log ages are not thresholded by default since they only
  // apply to FULL/BULK_LOGGED databases; see the docs for a recipe.
  filter_helper.add_options("full_age > 3d", "full_age < 0 or full_age > 7d", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} databases (${problem_list})", "${name}: last full backup ${full_age}s ago", "${name}",
                           "%(status): No databases found", "%(status): All %(count) databases have recent backups");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("include-copy-only", po::value<bool>(&include_copy_only)->implicit_value(true)->default_value(false),
      "Count COPY_ONLY backups when computing the ages. Excluded by default: an ad-hoc copy-only backup does not belong to the scheduled restore chain, so counting it would hide a failing backup job.")
    ("include-snapshot", po::value<bool>(&include_snapshot)->implicit_value(true)->default_value(false),
      "Count snapshot (VSS/third-party agent) backups when computing the ages. Excluded by default for the same reason.")
    ;
  // clang-format on
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(build_backup_sql(include_copy_only, include_snapshot));
    std::vector<backup_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      backup_row row;
      row.name = res.get_string(i, "name");
      row.recovery_model = res.get_string(i, "recovery_model");
      row.has_full = !res.is_null(i, "full_age");
      row.has_diff = !res.is_null(i, "diff_age");
      row.has_log = !res.is_null(i, "log_age");
      row.full_age = res.get_int(i, "full_age");
      row.diff_age = res.get_int(i, "diff_age");
      row.log_age = res.get_int(i, "log_age");
      rows.push_back(row);
    }

    for (const backup_info &db : build_backups(rows)) {
      auto record = std::make_shared<filter_obj>(db);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_backup_command
