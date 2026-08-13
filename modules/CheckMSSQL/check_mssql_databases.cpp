// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_databases.hpp"

#include <map>
#include <memory>
#include <nscapi/macros.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "mssql_filter_helpers.hpp"
#include "mssql_options.hpp"

namespace check_mssql_databases_command {

namespace {

// Three flat selects rather than one aggregating join: all the deciding (which
// file limits growth, how files roll up to a filegroup, what "unknown" means)
// happens in compute_headroom() where it is unit-testable, and each query can
// fail on its own. That matters because they need different permissions -
// sys.master_files needs only VIEW ANY DEFINITION, while dm_os_volume_stats
// needs VIEW SERVER STATE - so a login without the latter still gets sizes.

// ORDER BY name so the detail list and perfdata come out in a stable order; the
// GROUP BY this replaced used to impose one as a side effect.
const char *DATABASES_SQL =
    "SELECT d.database_id, d.name, d.state_desc AS state, d.recovery_model_desc AS recovery_model, d.is_read_only"
    " FROM sys.databases d ORDER BY d.name";

// max_size is a page count except for the -1 "unlimited" sentinel, which must
// survive the conversion to bytes. A log file with unlimited growth reports the
// 268435456-page (2TB) engine limit here rather than -1, and that is a real cap.
const char *FILES_SQL =
    "SELECT f.database_id, f.file_id, f.type, f.data_space_id, f.growth,"
    " CAST(f.size AS bigint) * 8192 AS size_bytes,"
    " CASE WHEN f.max_size < 0 THEN -1 ELSE CAST(f.max_size AS bigint) * 8192 END AS max_size_bytes"
    " FROM sys.master_files f WHERE f.type IN (0, 1)";

// OUTER APPLY, not CROSS: a file in an offline or inaccessible database yields
// no volume row and must still be visible as unknown rather than vanishing.
// available_bytes is per volume, so files sharing one must be recognized as
// such - a default 8-file tempdb reports the same free space eight times, and
// compute_headroom() counts each volume once. On Linux SQL Server all three
// volume identity columns (volume_mount_point, volume_id, logical_volume_name)
// come back NULL, so total_bytes is the fallback identity: it is stable, unlike
// available_bytes, and it can only merge two distinct volumes of exactly equal
// size, which under-reports headroom rather than multiplying it.
const char *VOLUMES_SQL =
    "SELECT f.database_id, f.file_id, vs.available_bytes,"
    " COALESCE(vs.volume_mount_point, vs.volume_id, CAST(vs.total_bytes AS varchar(30))) AS volume"
    " FROM sys.master_files f"
    " OUTER APPLY sys.dm_os_volume_stats(f.database_id, f.file_id) vs"
    " WHERE f.type IN (0, 1)";

typedef database_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  // Not type_size: the -1 unknown sentinel must be expressible (see
  // mssql_filter::parse_size), and type_size cannot compare against plain
  // integers at all.
  static const parsers::where::value_type type_headroom = parsers::where::type_custom_int_1;
  registry_.add_converter(type_headroom, &mssql_filter::parse_size<std::shared_ptr<filter_obj>>);

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
      .add_int_var("data_headroom", type_headroom, &filter_obj::get_data_headroom,
                   "Remaining growth room for the data files in bytes: each file's room is the distance to its max_size but never more than the free space on "
                   "its volume, summed per filegroup, and the most constrained filegroup wins; 0 when autogrowth is off, -1 if unavailable (supports units "
                   "and plain integers, e.g. data_headroom < 5G and data_headroom >= 0)")
      .add_int_perf("B", "", "_data_headroom")
      .add_int_var("log_headroom", type_headroom, &filter_obj::get_log_headroom,
                   "Remaining growth room for the log files in bytes, same semantics as data_headroom - note that a log file with unlimited growth still "
                   "carries the engine's 2TB cap, so this reports the volume's free space until the log approaches 2TB (supports units)")
      .add_int_perf("B", "", "_log_headroom")
      .add_int_var("is_read_only", &filter_obj::get_is_read_only, "1 if the database is read-only")
      .no_perf();
}

}  // namespace

std::map<long long, headroom_info> compute_headroom(const std::vector<file_row> &files) {
  // What the growable files sharing one volume can add to one filegroup.
  struct volume_budget {
    long long free = -1;       // free space on the volume
    long long capped_sum = 0;  // room left before max_size, over the capped files
    bool uncapped = false;     // some file can grow until the volume is full
    bool unknown = false;      // some file has no volume information
  };
  typedef std::tuple<long long, int, int> group_key;  // database, file type, filegroup
  std::map<std::tuple<long long, int, int, std::string>, volume_budget> budgets;
  std::map<group_key, long long> by_filegroup;

  for (const file_row &file : files) {
    // Register the filegroup even when nothing in it can grow: a filegroup of
    // fixed-size files has no room, which is a value, not a missing entry.
    by_filegroup.insert(std::make_pair(std::make_tuple(file.database_id, file.type, file.data_space_id), 0LL));
    if (file.growth == 0 || file.max_size_bytes == 0) continue;  // autogrowth off, or pinned at its current size
    volume_budget &budget = budgets[std::make_tuple(file.database_id, file.type, file.data_space_id, file.volume)];
    if (file.volume.empty() || file.available_bytes < 0) {
      budget.unknown = true;
      continue;
    }
    // Files on one volume report identical free space; take the smallest anyway,
    // so that the size-based identity fallback merging two same-sized volumes
    // under-reports rather than over-reports.
    if (budget.free < 0 || file.available_bytes < budget.free) budget.free = file.available_bytes;
    if (file.max_size_bytes < 0) {
      budget.uncapped = true;  // only the volume limits it
    } else {
      const long long to_cap = file.max_size_bytes - file.size_bytes;
      if (to_cap > 0) budget.capped_sum += to_cap;  // at or past a lowered cap adds nothing
    }
  }

  for (const auto &entry : budgets) {
    const group_key group(std::get<0>(entry.first), std::get<1>(entry.first), std::get<2>(entry.first));
    long long &room = by_filegroup[group];
    if (room < 0) continue;  // already unknown
    const volume_budget &budget = entry.second;
    if (budget.unknown) {
      room = -1;
      continue;
    }
    room += budget.uncapped || budget.free < budget.capped_sum ? budget.free : budget.capped_sum;
  }

  // (database, file type) -> the most constrained filegroup, unknown if any is.
  std::map<std::pair<long long, int>, long long> by_type;
  for (const auto &group : by_filegroup) {
    const std::pair<long long, int> key(std::get<0>(group.first), std::get<1>(group.first));
    const long long room = group.second;
    const auto it = by_type.find(key);
    if (it == by_type.end())
      by_type[key] = room;
    else if (it->second >= 0)
      it->second = (room < 0 || room < it->second) ? room : it->second;
  }

  std::map<long long, headroom_info> result;
  for (const auto &entry : by_type) {
    headroom_info &info = result[entry.first.first];
    if (entry.first.second == 0)
      info.data = entry.second;
    else
      info.log = entry.second;
  }
  return result;
}

databases_type build_databases(const std::vector<database_row> &databases, const std::vector<logspace_row> &logspace, const std::vector<file_row> &files) {
  std::unordered_map<std::string, long long> log_used_by_name;
  for (const logspace_row &ls : logspace) log_used_by_name[ls.name] = ls.used_pct;
  std::unordered_map<long long, long long> data_bytes, log_bytes;
  for (const file_row &file : files) (file.type == 0 ? data_bytes : log_bytes)[file.database_id] += file.size_bytes;
  const std::map<long long, headroom_info> headroom = compute_headroom(files);

  databases_type result;
  for (const database_row &row : databases) {
    database_info info;
    info.name = row.name;
    info.state = row.state;
    info.recovery_model = row.recovery_model;
    info.is_read_only = row.is_read_only;
    const auto data_it = data_bytes.find(row.database_id);
    if (data_it != data_bytes.end()) info.data_size = data_it->second;
    const auto log_it = log_bytes.find(row.database_id);
    if (log_it != log_bytes.end()) info.log_size = log_it->second;
    const auto used_it = log_used_by_name.find(row.name);
    if (used_it != log_used_by_name.end()) info.log_used_pct = used_it->second;
    const auto room_it = headroom.find(row.database_id);
    if (room_it != headroom.end()) {
      info.data_headroom = room_it->second.data;
      info.log_headroom = room_it->second.log;
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
      row.database_id = res.get_int(i, "database_id");
      row.name = res.get_string(i, "name");
      row.state = res.get_string(i, "state");
      row.recovery_model = res.get_string(i, "recovery_model");
      row.is_read_only = res.get_int(i, "is_read_only") != 0;
      databases.push_back(row);
    }

    // Log usage is nice-to-have: if DBCC SQLPERF is denied or fails, keep the
    // check working and report log_used_pct = -1.
    std::vector<logspace_row> logspace;
    try {
      const mssql_odbc::result ls = session.execute("DBCC SQLPERF(LOGSPACE)");
      // Read by position: the column names (Database Name, Log Size (MB),
      // Log Space Used (%), Status) are localized on non-English servers,
      // but the shape has been stable across SQL Server versions.
      if (ls.columns.size() == 4) {
        for (std::size_t i = 0; i < ls.rows.size(); i++) {
          logspace_row row;
          row.name = ls.get_string(i, 0);
          row.used_pct = ls.get_int(i, 2);
          logspace.push_back(row);
        }
      } else {
        NSC_DEBUG_MSG("DBCC SQLPERF(LOGSPACE) returned " + std::to_string(ls.columns.size()) + " columns (expected 4), log_used_pct will be unavailable");
      }
    } catch (const mssql_odbc::odbc_exception &e) {
      NSC_DEBUG_MSG("DBCC SQLPERF(LOGSPACE) failed, log_used_pct will be unavailable: " + e.reason());
    }

    // Volume free space is likewise nice-to-have: dm_os_volume_stats needs VIEW
    // SERVER STATE, so a file with no entry here reports headroom -1 while its
    // size still comes through.
    std::map<std::pair<long long, long long>, std::pair<std::string, long long>> volume_by_file;
    try {
      const mssql_odbc::result vs = session.execute(VOLUMES_SQL);
      for (std::size_t i = 0; i < vs.rows.size(); i++) {
        if (vs.is_null(i, "available_bytes") || vs.is_null(i, "volume")) continue;
        volume_by_file[std::make_pair(vs.get_int(i, "database_id"), vs.get_int(i, "file_id"))] =
            std::make_pair(vs.get_string(i, "volume"), vs.get_int(i, "available_bytes"));
      }
    } catch (const mssql_odbc::odbc_exception &e) {
      NSC_DEBUG_MSG("volume stats query failed, data/log_headroom will be unavailable: " + e.reason());
    }

    const mssql_odbc::result fs = session.execute(FILES_SQL);
    std::vector<file_row> files;
    for (std::size_t i = 0; i < fs.rows.size(); i++) {
      file_row row;
      row.database_id = fs.get_int(i, "database_id");
      row.file_id = fs.get_int(i, "file_id");
      row.type = static_cast<int>(fs.get_int(i, "type"));
      row.data_space_id = static_cast<int>(fs.get_int(i, "data_space_id"));
      row.growth = fs.get_int(i, "growth");
      row.size_bytes = fs.get_int(i, "size_bytes");
      row.max_size_bytes = fs.get_int(i, "max_size_bytes");
      const auto vol = volume_by_file.find(std::make_pair(row.database_id, row.file_id));
      if (vol != volume_by_file.end()) {
        row.volume = vol->second.first;
        row.available_bytes = vol->second.second;
      }
      files.push_back(row);
    }

    for (const database_info &db : build_databases(databases, logspace, files)) {
      auto record = std::make_shared<filter_obj>(db);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_databases_command
