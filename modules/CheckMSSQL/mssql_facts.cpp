// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mssql_facts.hpp"

#include <algorithm>
#include <cctype>
#include <nscapi/nscapi_facts_helper.hpp>

namespace mssql_facts {

const char *const set_mssql = "mssql";
const char *const id_server = "mssql";
const char *const id_databases = "mssql.databases";
const char *const key_databases = "databases";
const std::size_t max_records = 2500;

// SERVERPROPERTY() only: it exists on every supported version, and a property
// the version does not know comes back NULL instead of failing the statement.
const char *const SERVER_SQL =
    "SELECT CAST(SERVERPROPERTY('ServerName') AS nvarchar(256)) AS server_name,"
    " CAST(SERVERPROPERTY('MachineName') AS nvarchar(256)) AS machine_name,"
    " CAST(SERVERPROPERTY('InstanceName') AS nvarchar(256)) AS instance_name,"
    " CAST(SERVERPROPERTY('ProductVersion') AS nvarchar(128)) AS version,"
    " CAST(SERVERPROPERTY('ProductLevel') AS nvarchar(128)) AS product_level,"
    " CAST(SERVERPROPERTY('ProductUpdateLevel') AS nvarchar(128)) AS product_update_level,"
    " CAST(SERVERPROPERTY('Edition') AS nvarchar(128)) AS edition,"
    " CAST(SERVERPROPERTY('EngineEdition') AS int) AS engine_edition,"
    " CAST(SERVERPROPERTY('Collation') AS nvarchar(128)) AS collation,"
    " CAST(SERVERPROPERTY('IsClustered') AS int) AS is_clustered,"
    " CAST(SERVERPROPERTY('IsHadrEnabled') AS int) AS is_hadr_enabled,"
    " CAST(SERVERPROPERTY('IsIntegratedSecurityOnly') AS int) AS is_integrated_security_only";
// The date is rendered by the server (style 23 is YYYY-MM-DD) so the record
// does not depend on how the ODBC driver spells a datetime. ORDER BY so the
// list is stable across rounds whatever the storage order.
const char *const DATABASES_SQL =
    "SELECT name, recovery_model_desc AS recovery_model, collation_name AS collation, compatibility_level,"
    " CONVERT(nvarchar(10), create_date, 23) AS create_date, is_read_only"
    " FROM sys.databases ORDER BY name";

namespace {
// A NULL cell reads as "" from get_string and as 0 from get_int, which is
// the same "not known" the record omits; only the flags below need to tell
// NULL from false.
// A property the server did not answer (NULL) leaves the flag unknown,
// which is omitted; anything else is a yes or a no.
void read_flag(const mssql_odbc::result &result, const std::string &column, bool &known, bool &value) {
  known = !result.is_null(0, column);
  value = known && result.get_int(0, column) != 0;
}
bool looks_like_date(const std::string &text) {
  if (text.size() != 10 || text[4] != '-' || text[7] != '-') return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (i == 4 || i == 7) continue;
    if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false;
  }
  return true;
}
}  // namespace

std::string engine_edition_name(const long long engine_edition) {
  switch (engine_edition) {
    case 1:
      return "personal";
    case 2:
      return "standard";
    case 3:
      return "enterprise";
    case 4:
      return "express";
    case 5:
      return "azure_sql_database";
    case 6:
      return "azure_synapse_analytics";
    case 8:
      return "azure_sql_managed_instance";
    case 9:
      return "azure_sql_edge";
    case 11:
      return "azure_synapse_serverless";
    default:
      return engine_edition > 0 ? std::to_string(engine_edition) : "";
  }
}

server parse_server(const mssql_odbc::result &result) {
  if (result.rows.empty()) throw mssql_odbc::odbc_exception("Server returned no server properties");
  server s;
  s.server_name = result.get_string(0, "server_name");
  s.machine_name = result.get_string(0, "machine_name");
  s.instance_name = result.get_string(0, "instance_name");
  s.version = result.get_string(0, "version");
  s.product_level = result.get_string(0, "product_level");
  s.product_update_level = result.get_string(0, "product_update_level");
  s.edition = result.get_string(0, "edition");
  s.engine_edition = engine_edition_name(result.get_int(0, "engine_edition"));
  s.collation = result.get_string(0, "collation");
  if (!result.is_null(0, "is_integrated_security_only")) {
    s.authentication = result.get_int(0, "is_integrated_security_only") != 0 ? "windows" : "mixed";
  }
  read_flag(result, "is_clustered", s.has_clustered, s.clustered);
  read_flag(result, "is_hadr_enabled", s.has_always_on, s.always_on);
  return s;
}

std::vector<database> parse_databases(const mssql_odbc::result &result) {
  std::vector<database> databases;
  for (std::size_t i = 0; i < result.rows.size(); ++i) {
    database d;
    d.id = result.get_string(i, "name");
    if (d.id.empty()) continue;  // not a database the server can name; nothing to record it by
    d.recovery_model = result.get_string(i, "recovery_model");
    d.collation = result.get_string(i, "collation");
    d.compatibility_level = result.get_int(i, "compatibility_level");
    const std::string created = result.get_string(i, "create_date");
    if (looks_like_date(created)) d.create_date = created;
    d.read_only = result.get_int(i, "is_read_only") != 0;
    databases.push_back(d);
  }
  // Sorted here as well as in the query, so the document does not depend on
  // the server's collation of database names.
  std::sort(databases.begin(), databases.end(), [](const database &a, const database &b) { return a.id < b.id; });
  return databases;
}

snapshot gather(const selection &what, const query_runner &run) {
  snapshot snap;
  if (what.server) snap.server_info = parse_server(run(SERVER_SQL));
  if (what.databases) snap.databases = parse_databases(run(DATABASES_SQL));
  return snap;
}

void publish(const selection &what, const snapshot &snap, const std::time_t taken_at, nscapi::facts::response &out) {
  if (!what.any()) return;
  nscapi::facts::section mssql = out.set(set_mssql);
  if (what.server) {
    const server &s = snap.server_info;
    mssql.value("server_name", s.server_name)
        .value("machine_name", s.machine_name)
        .value("instance_name", s.instance_name)
        .value("version", s.version)
        .value("product_level", s.product_level)
        .value("product_update_level", s.product_update_level)
        .value("edition", s.edition)
        .value("engine_edition", s.engine_edition)
        .value("collation", s.collation)
        .value("authentication", s.authentication);
    // A flag the server did not answer is unknown, and unknown is omitted.
    if (s.has_clustered) mssql.value("clustered", s.clustered);
    if (s.has_always_on) mssql.value("always_on", s.always_on);
  }
  if (what.databases) {
    // Written even when empty: a server with no database the login may see
    // has told us something (most likely about the login), and an absent
    // list would read as "not collected".
    nscapi::facts::record_list list = mssql.list(key_databases);
    const std::size_t count = std::min(snap.databases.size(), max_records);
    for (std::size_t n = 0; n < count; ++n) {
      const database &d = snap.databases[n];
      nscapi::facts::section record = list.record(d.id);
      record.value("recovery_model", d.recovery_model).value("collation", d.collation);
      if (d.compatibility_level > 0) record.value("compatibility_level", d.compatibility_level);
      record.value("create_date", d.create_date);
      record.value("read_only", d.read_only);
    }
    if (snap.databases.size() > count) {
      // The set is still published: most of an inventory, and the reason it
      // is not all of it, beats the core rejecting it over the size budget.
      out.error(set_mssql, "Instance has " + std::to_string(snap.databases.size()) + " databases; only the first " + std::to_string(count) +
                               " are reported, to keep the facts document inside its size budget");
    }
  }
  out.gathered(set_mssql, taken_at);
}

}  // namespace mssql_facts
