// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_availability_groups.hpp"

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_options.hpp"

namespace check_mssql_availability_groups_command {

namespace {

// One row per (group, replica, database). The replica-state join is INNER on
// purpose: a secondary only has state rows for its local replica, and keeping
// the catalog-only remote replicas would report them DISCONNECTED/NOT_HEALTHY
// on every secondary. Run the check against the primary for the full picture
// (send/redo queues of every secondary); a secondary sees its local state.
// Queue sizes are reported by the DMV in KB and exposed in bytes so size
// units (redo_queue > 500M) work like everywhere else.
const char *AVAILABILITY_GROUPS_SQL =
    "SELECT ag.name AS group_name, ar.replica_server_name AS replica,"
    " ars.role_desc AS role,"
    " ISNULL(ars.connected_state_desc, 'DISCONNECTED') AS connected_state,"
    " ISNULL(ars.synchronization_health_desc, 'NOT_HEALTHY') AS replica_health,"
    " ISNULL(ars.is_local, 0) AS is_local,"
    " ISNULL(DB_NAME(drs.database_id), '') AS database_name,"
    " ISNULL(drs.synchronization_state_desc, '') AS sync_state,"
    " ISNULL(drs.synchronization_health_desc, '') AS db_health,"
    " ISNULL(drs.redo_queue_size, 0) * 1024 AS redo_queue,"
    " ISNULL(drs.log_send_queue_size, 0) * 1024 AS log_send_queue,"
    " ISNULL(drs.is_suspended, 0) AS is_suspended"
    " FROM sys.availability_groups ag"
    " JOIN sys.availability_replicas ar ON ar.group_id = ag.group_id"
    " JOIN sys.dm_hadr_availability_replica_states ars ON ars.replica_id = ar.replica_id AND ars.group_id = ag.group_id"
    " LEFT JOIN sys.dm_hadr_database_replica_states drs ON drs.replica_id = ar.replica_id AND drs.group_id = ag.group_id"
    " ORDER BY ag.name, ar.replica_server_name, database_name";

typedef replica_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "group/replica or group/replica/database")
      .add_string_var("group", &filter_obj::get_group, "Availability group name")
      .add_string_var("replica", &filter_obj::get_replica, "Replica server name")
      .add_string_var("role", &filter_obj::get_role, "Replica role: PRIMARY, SECONDARY or RESOLVING (no primary, e.g. during failover)")
      .add_string_var("connected_state", &filter_obj::get_connected_state, "CONNECTED or DISCONNECTED")
      .add_string_var("replica_health", &filter_obj::get_replica_health, "Replica synchronization health: HEALTHY, PARTIALLY_HEALTHY or NOT_HEALTHY")
      .add_string_var("health", &filter_obj::get_health, "Effective health: database health for database rows, replica health otherwise")
      .add_string_var("database", &filter_obj::get_database, "Availability database name (empty for replica-level rows)")
      .add_string_var("sync_state", &filter_obj::get_sync_state, "Database synchronization state, e.g. SYNCHRONIZED or SYNCHRONIZING")
      .add_string_var("db_health", &filter_obj::get_db_health, "Database synchronization health (empty for replica-level rows)");

  registry_
      .add_int_var("redo_queue", parsers::where::type_size, &filter_obj::get_redo_queue,
                   "Redo queue on the secondary in bytes: log received but not yet applied, i.e. failover/RTO lag (supports units, e.g. redo_queue > 500M)")
      .add_int_perf("B", "", "_redo_queue")
      .add_int_var("log_send_queue", parsers::where::type_size, &filter_obj::get_log_send_queue,
                   "Log not yet sent to the secondary in bytes, i.e. potential data loss/RPO lag (supports units, e.g. log_send_queue > 100M)")
      .add_int_perf("B", "", "_log_send_queue")
      .add_int_var("is_local", &filter_obj::get_is_local, "1 if this replica is the instance being checked")
      .no_perf()
      .add_int_var("is_suspended", &filter_obj::get_is_suspended, "1 if data movement for the database is suspended")
      .no_perf();
}

}  // namespace

replicas_type build_replicas(const std::vector<replica_row> &rows) {
  replicas_type result;
  for (const replica_row &row : rows) {
    replica_info info;
    info.group = row.group;
    info.replica = row.replica;
    info.role = row.role;
    info.connected_state = row.connected_state;
    info.replica_health = row.replica_health;
    info.is_local = row.is_local ? 1 : 0;
    info.database = row.database;
    info.sync_state = row.sync_state;
    info.db_health = row.db_health;
    info.redo_queue = row.redo_queue;
    info.log_send_queue = row.log_send_queue;
    info.is_suspended = row.is_suspended ? 1 : 0;
    info.name = row.group + "/" + row.replica + (row.database.empty() ? "" : "/" + row.database);
    info.health = row.database.empty() ? row.replica_health : row.db_health;
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
  // An instance without availability groups reports OK by default so the check
  // can be rolled out fleet-wide; set empty-state=critical on hosts where an
  // AG must exist (a dropped AG silently removes the protection it provided).
  filter_helper.add_options("health = 'PARTIALLY_HEALTHY'",
                            "health = 'NOT_HEALTHY' or connected_state = 'DISCONNECTED' or is_suspended = 1 or role = 'RESOLVING'", "",
                            filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} availability replicas/databases (${problem_list})", "${name}: ${health}", "${name}",
                           "%(status): No availability groups found", "%(status): All %(count) availability replicas/databases are healthy");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(AVAILABILITY_GROUPS_SQL);
    std::vector<replica_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      replica_row row;
      row.group = res.get_string(i, "group_name");
      row.replica = res.get_string(i, "replica");
      row.role = res.get_string(i, "role");
      row.connected_state = res.get_string(i, "connected_state");
      row.replica_health = res.get_string(i, "replica_health");
      row.is_local = res.get_int(i, "is_local") != 0;
      row.database = res.get_string(i, "database_name");
      row.sync_state = res.get_string(i, "sync_state");
      row.db_health = res.get_string(i, "db_health");
      row.redo_queue = res.get_int(i, "redo_queue");
      row.log_send_queue = res.get_int(i, "log_send_queue");
      row.is_suspended = res.get_int(i, "is_suspended") != 0;
      rows.push_back(row);
    }

    for (const replica_info &replica : build_replicas(rows)) {
      auto record = std::make_shared<filter_obj>(replica);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_availability_groups_command
