// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mysql_facts.hpp"

#include <algorithm>
#include <facts/host_facts.hpp>
#include <nscapi/nscapi_facts_helper.hpp>

namespace mysql_facts {

const char *const set_mysql = "mysql";
const char *const id_server = "mysql";
const char *const id_databases = "mysql.databases";
const char *const key_databases = "databases";
const std::size_t max_records = 2500;

// Every variable here exists on MySQL 5.7+, MariaDB and Percona alike, so one
// statement serves every flavor; a server that lacked one would fail the
// whole statement, which is reported rather than half-published.
const char *const SERVER_SQL =
    "SELECT @@version AS version, @@version_comment AS version_comment, @@hostname AS hostname, @@port AS port, @@server_id AS server_id, "
    "@@character_set_server AS character_set, @@collation_server AS collation, @@version_compile_os AS os, @@version_compile_machine AS machine";
// ORDER BY so the list is stable across rounds whatever the storage order.
const char *const DATABASES_SQL =
    "SELECT SCHEMA_NAME AS name, DEFAULT_CHARACTER_SET_NAME AS character_set, DEFAULT_COLLATION_NAME AS collation FROM information_schema.SCHEMATA "
    "ORDER BY SCHEMA_NAME";

// A NULL cell reads as "" from get_string and as 0 from get_int, which is
// the same "not known" the record omits; no separate NULL check is needed.

server parse_server(const mysql_client::result &result) {
  if (result.rows.empty()) throw mysql_client::mysql_exception("Server returned no version information");
  server s;
  s.version = result.get_string(0, "version");
  s.version_comment = result.get_string(0, "version_comment");
  s.flavor = mysql_client::derive_flavor(s.version, s.version_comment);
  s.hostname = result.get_string(0, "hostname");
  s.port = result.get_int(0, "port");
  s.server_id = result.get_int(0, "server_id");
  s.character_set = result.get_string(0, "character_set");
  s.collation = result.get_string(0, "collation");
  s.os = result.get_string(0, "os");
  // The server spells it as its build did (`x86_64`, `aarch64`); the `os`
  // set has one spelling per architecture and this has to match it.
  s.architecture = host_facts::normalize_arch(result.get_string(0, "machine"));
  return s;
}

std::vector<database> parse_databases(const mysql_client::result &result) {
  std::vector<database> databases;
  for (std::size_t i = 0; i < result.rows.size(); ++i) {
    database d;
    d.id = result.get_string(i, "name");
    if (d.id.empty()) continue;  // not a schema the server can name; nothing to record it by
    d.character_set = result.get_string(i, "character_set");
    d.collation = result.get_string(i, "collation");
    databases.push_back(d);
  }
  // Sorted here as well as in the query, so the document does not depend on
  // the server's collation of schema names.
  std::sort(databases.begin(), databases.end(), [](const database &a, const database &b) { return a.id < b.id; });
  return databases;
}

snapshot gather(const selection &what, const mysql_client::query_runner &run) {
  snapshot snap;
  if (what.server) snap.server_info = parse_server(run(SERVER_SQL));
  if (what.databases) snap.databases = parse_databases(run(DATABASES_SQL));
  return snap;
}

void publish(const selection &what, const snapshot &snap, const std::time_t taken_at, nscapi::facts::response &out) {
  if (!what.any()) return;
  nscapi::facts::section mysql = out.set(set_mysql);
  if (what.server) {
    const server &s = snap.server_info;
    mysql.value("flavor", s.flavor).value("version", s.version).value("version_comment", s.version_comment).value("hostname", s.hostname);
    // Zero is "not reported", and the builder writes a number as it is
    // given, so the guard is here.
    if (s.port > 0) mysql.value("port", s.port);
    if (s.server_id > 0) mysql.value("server_id", s.server_id);
    mysql.value("character_set", s.character_set).value("collation", s.collation).value("os", s.os).value("architecture", s.architecture);
  }
  if (what.databases) {
    // Written even when empty: a server with no schema we may see has told
    // us something (most likely about the grant), and an absent list would
    // read as "not collected".
    nscapi::facts::record_list list = mysql.list(key_databases);
    const std::size_t count = std::min(snap.databases.size(), max_records);
    for (std::size_t n = 0; n < count; ++n) {
      const database &d = snap.databases[n];
      list.record(d.id).value("character_set", d.character_set).value("collation", d.collation);
    }
    if (snap.databases.size() > count) {
      // The set is still published: most of an inventory, and the reason it
      // is not all of it, beats the core rejecting it over the size budget.
      out.error(set_mysql, "Server has " + std::to_string(snap.databases.size()) + " databases; only the first " + std::to_string(count) +
                               " are reported, to keep the facts document inside its size budget");
    }
  }
  out.gathered(set_mysql, taken_at);
}

}  // namespace mysql_facts
