// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_tempdb.hpp"

#include <memory>
#include <nscapi/macros.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_options.hpp"

namespace check_mssql_tempdb_command {

namespace {

// Space usage by consumer across all tempdb data files (pages are 8KB). The
// three-way split is the diagnostic: version store growth means a long
// snapshot transaction is pinning it, internal objects mean query spills /
// work tables, user objects mean temp tables. The DMV is queried through the
// tempdb.sys. prefix so the check works from any database context.
const char *TEMPDB_SQL =
    "SELECT"
    " ISNULL(SUM(CAST(total_page_count AS bigint)), 0) * 8192 AS size,"
    " ISNULL(SUM(CAST(unallocated_extent_page_count AS bigint)), 0) * 8192 AS free,"
    " ISNULL(SUM(CAST(version_store_reserved_page_count AS bigint)), 0) * 8192 AS version_store,"
    " ISNULL(SUM(CAST(user_object_reserved_page_count AS bigint)), 0) * 8192 AS user_objects,"
    " ISNULL(SUM(CAST(internal_object_reserved_page_count AS bigint)), 0) * 8192 AS internal_objects"
    " FROM tempdb.sys.dm_db_file_space_usage";

// Free space on the most constrained volume holding a tempdb data file: MIN
// because files on the fullest volume hit the wall first, regardless of how
// much room the others have.
const char *TEMPDB_VOLUME_SQL =
    "SELECT MIN(CAST(vs.available_bytes AS bigint)) AS volume_free"
    " FROM tempdb.sys.database_files f"
    " CROSS APPLY sys.dm_os_volume_stats(2, f.file_id) vs"
    " WHERE f.type = 0";

typedef tempdb_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_
      .add_int_var("size", parsers::where::type_size, &filter_obj::get_size, "Allocated tempdb data-file bytes (supports units, e.g. size > 50G)")
      .add_int_perf("B", "", "_size")
      .add_int_var("free", parsers::where::type_size, &filter_obj::get_free, "Unallocated bytes within the tempdb files (supports units)")
      .add_int_perf("B", "", "_free")
      .add_int_var("used", parsers::where::type_size, &filter_obj::get_used, "Bytes in use within the tempdb files (supports units)")
      .add_int_perf("B", "", "_used")
      .add_int_var("used_pct", &filter_obj::get_used_pct, "Percent of the current tempdb allocation in use")
      .add_int_perf("%", "", "_used_pct")
      .add_int_var("version_store", parsers::where::type_size, &filter_obj::get_version_store,
                   "Bytes held by the version store; growth means a long-running snapshot transaction is pinning it (supports units)")
      .add_int_perf("B", "", "_version_store")
      .add_int_var("user_objects", parsers::where::type_size, &filter_obj::get_user_objects,
                   "Bytes held by user objects: temp tables and table variables (supports units)")
      .add_int_perf("B", "", "_user_objects")
      .add_int_var("internal_objects", parsers::where::type_size, &filter_obj::get_internal_objects,
                   "Bytes held by internal objects: sort/hash spills and work tables (supports units)")
      .add_int_perf("B", "", "_internal_objects")
      .add_int_var("volume_free", parsers::where::type_size, &filter_obj::get_volume_free,
                   "Free bytes on the most constrained volume holding a tempdb data file, -1 = unknown (supports units, e.g. volume_free < 5G)")
      .add_int_perf("B", "", "_volume_free");
}

}  // namespace

tempdb_info build_tempdb(const tempdb_row &row) {
  tempdb_info info;
  info.size = row.size;
  info.free = row.free;
  info.used = row.size - row.free;
  info.used_pct = row.size > 0 ? (100 * info.used) / row.size : 0;
  info.version_store = row.version_store;
  info.user_objects = row.user_objects;
  info.internal_objects = row.internal_objects;
  info.volume_free = row.volume_free;
  return info;
}

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;

  filter_type filter;
  // No default thresholds: used_pct of the current allocation is soft when
  // autogrowth is on, and pre-sized tempdb capacity is a sizing decision.
  // The docs suggest used_pct and volume_free starting points.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}",
                           "tempdb ${used_pct}% used of ${size}B (version store ${version_store}B, user ${user_objects}B, internal ${internal_objects}B)",
                           "tempdb", "%(status): No tempdb information returned", "");
  // tempdb trends are the point of the check: emit everything as perfdata.
  filter_helper.set_default_perf_config("extra(size;free;used;used_pct;version_store;user_objects;internal_objects;volume_free)");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(TEMPDB_SQL);
    if (res.rows.empty()) {
      filter_helper.post_process(filter);
      return;
    }
    tempdb_row row;
    row.size = res.get_int(0, "size");
    row.free = res.get_int(0, "free");
    row.version_store = res.get_int(0, "version_store");
    row.user_objects = res.get_int(0, "user_objects");
    row.internal_objects = res.get_int(0, "internal_objects");

    // Volume stats are nice-to-have: if the DMF is denied or unavailable keep
    // the check working and report volume_free = -1.
    try {
      const mssql_odbc::result vol = session.execute(TEMPDB_VOLUME_SQL);
      if (!vol.rows.empty() && !vol.is_null(0, "volume_free")) row.volume_free = vol.get_int(0, "volume_free");
    } catch (const mssql_odbc::odbc_exception &e) {
      NSC_DEBUG_MSG("dm_os_volume_stats unavailable, volume_free will be -1: " + e.reason());
    }

    auto record = std::make_shared<filter_obj>(build_tempdb(row));
    filter.match(record);
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_tempdb_command
