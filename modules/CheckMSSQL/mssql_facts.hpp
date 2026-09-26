// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

// The `mssql` fact set: which SQL Server instance this module is pointed at,
// and which databases it holds.
//
// The inventory, not the monitoring: the instance's version, edition and
// patch level are here, its uptime is not; a database is listed with its
// recovery model and collation, never with its state or its size. Those move
// every round, a value that moves every round bumps the document's revision
// every round, and check_mssql / check_mssql_databases are where they live.
//
// Describes the instance, not how the agent reaches it. The record carries
// the names the *server* reports about itself (SERVERPROPERTY), never the
// configured target, the login or the connection string. Facts never contain
// settings or credentials.
//
// The server record is read with SERVERPROPERTY() alone, which every SQL
// Server answers and which returns NULL rather than failing for a property a
// version does not have, so one statement serves 2008 and 2022 alike and an
// unknown property is an omitted field. The queries arrive through an
// injectable runner, so the parsing is unit-tested against canned result
// sets without an ODBC driver.
namespace mssql_facts {

// The fact set this module produces, and the two enableable ids in it. The
// ids are the settings keys under [/settings/mssql/facts]: `mssql` is the
// server record, `mssql.databases` the list.
extern const char *const set_mssql;
extern const char *const id_server;
extern const char *const id_databases;
extern const char *const key_databases;

// Which of the two an operator turned on. gather() runs only the queries it
// needs, and publish() writes only that.
struct selection {
  bool server = false;
  bool databases = false;
  bool any() const { return server || databases; }
};

// Executes one statement against a connected session. The module binds it to
// an mssql_odbc::session; a test binds it to canned results.
typedef std::function<mssql_odbc::result(const std::string &sql)> query_runner;

// The instance, as SERVERPROPERTY() describes it. Empty strings and zero
// numbers mean "not known" and are omitted from the record; the booleans
// carry a flag saying whether the server answered them.
struct server {
  std::string server_name;           // ServerName: `DB01\PROD`, the `server_name` keyword of check_mssql
  std::string machine_name;          // MachineName: the Windows computer name, or the cluster network name
  std::string instance_name;         // InstanceName: `PROD`; absent for the default instance
  std::string version;               // ProductVersion: `16.0.1000.6`
  std::string product_level;         // ProductLevel: `RTM`, `SP3`
  std::string product_update_level;  // ProductUpdateLevel: `CU12`
  std::string edition;               // Edition: `Express Edition (64-bit)`
  std::string engine_edition;        // EngineEdition, named: `standard`, `enterprise`, `express`, `azure_sql_database`, …
  std::string collation;             // Collation: `SQL_Latin1_General_CP1_CI_AS`
  std::string authentication;        // IsIntegratedSecurityOnly, named: `windows` or `mixed`
  bool has_clustered = false;
  bool clustered = false;  // IsClustered
  bool has_always_on = false;
  bool always_on = false;  // IsHadrEnabled
};

// One database, as sys.databases lists it.
struct database {
  std::string id;              // the database name: the `name` keyword of check_mssql_databases
  std::string recovery_model;  // SIMPLE, FULL or BULK_LOGGED, as the check spells it
  std::string collation;
  long long compatibility_level = 0;  // 160 for SQL Server 2022
  std::string create_date;            // YYYY-MM-DD
  bool read_only = false;
};

// Everything one round collected. Gathered whole before anything is
// published, so a round whose second query failed does not replace a set
// that has both parts with one that has only the first.
struct snapshot {
  server server_info;
  std::vector<database> databases;
};

// The queries, exposed so a test's fake session can recognise them.
extern const char *const SERVER_SQL;
extern const char *const DATABASES_SQL;

// The EngineEdition vocabulary. A number this does not know is returned as
// its digits, so a new edition publishes something honest rather than
// nothing.
std::string engine_edition_name(long long engine_edition);

// Build the parts of a snapshot from their result sets. Pure; a result set
// without the expected row or columns throws mssql_odbc::odbc_exception.
server parse_server(const mssql_odbc::result &result);
std::vector<database> parse_databases(const mssql_odbc::result &result);

// Run what `what` asks for on the connected session behind `run`. Throws
// mssql_odbc::odbc_exception on a failed query and never returns a partial
// snapshot.
snapshot gather(const selection &what, const query_runner &run);

// Add the `mssql` set, with the parts of `snap` that `what` selects, to
// `out`. `taken_at` stamps when the values were read.
void publish(const selection &what, const snapshot &snap, std::time_t taken_at, nscapi::facts::response &out);

}  // namespace mssql_facts
