// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "mysql_client.hpp"

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

// The `mysql` fact set: which MySQL-compatible server this module is pointed
// at, and which databases it holds.
//
// The inventory, not the monitoring: the server's version and flavor are
// here, its uptime and connection count are not. Those move every round, a
// value that moves every round bumps the document's revision every round,
// and check_mysql is where they live. For the same reason a database is
// listed with its character set, never with its size.
//
// Describes the server, not how the agent reaches it. The record carries the
// hostname and port the *server* reports about itself (@@hostname, @@port),
// never the configured target, and never the user or anything else from
// [/settings/mysql]. Facts never contain settings or credentials.
//
// Platform-neutral throughout, like the checks: the queries are the data
// source on every platform, they arrive through the module's injectable
// session, and the parsing is unit-tested against canned result sets.
namespace mysql_facts {

// The fact set this module produces, and the two enableable ids in it. The
// ids are the settings keys under [/settings/mysql/facts]: `mysql` is the
// server record, `mysql.databases` the list.
extern const char *const set_mysql;
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

// The server, as its own system variables describe it. Empty strings and
// zero numbers mean "not known" and are omitted from the record.
struct server {
  std::string flavor;           // mysql, mariadb or percona: the `flavor` keyword of check_mysql
  std::string version;          // @@version: `10.11.14-MariaDB-ubu2404`, `8.4.3`
  std::string version_comment;  // @@version_comment
  std::string hostname;         // @@hostname: what the server calls the machine it runs on
  long long port = 0;           // @@port
  long long server_id = 0;      // @@server_id, the replication identity
  std::string character_set;    // @@character_set_server
  std::string collation;        // @@collation_server
  std::string os;               // @@version_compile_os: `Linux`, `Win64`, `debian-linux-gnu`
  std::string architecture;     // @@version_compile_machine, normalised to the `os` set's vocabulary
};

// One database (schema), as information_schema.SCHEMATA lists it. System
// schemas are listed like any other: they are databases the server has.
struct database {
  std::string id;  // the schema name
  std::string character_set;
  std::string collation;
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

// Build the parts of a snapshot from their result sets. Pure; a result set
// without the expected row or columns throws mysql_client::mysql_exception.
server parse_server(const mysql_client::result &result);
std::vector<database> parse_databases(const mysql_client::result &result);

// Run what `what` asks for on the connected session behind `run`. Throws
// mysql_client::mysql_exception on a failed query and never returns a partial
// snapshot.
snapshot gather(const selection &what, const mysql_client::query_runner &run);

// Add the `mysql` set, with the parts of `snap` that `what` selects, to
// `out`. `taken_at` stamps when the values were read.
void publish(const selection &what, const snapshot &snap, std::time_t taken_at, nscapi::facts::response &out);

}  // namespace mysql_facts
