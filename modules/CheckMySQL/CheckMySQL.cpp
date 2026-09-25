// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckMySQL.h"

#include <ctime>
#include <nscapi/macros.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

#include "check_mysql.hpp"
#include "check_mysql_query.hpp"
#include "mysql_facts.hpp"
#include "mysql_session.hpp"

namespace sh = nscapi::settings_helper;

CheckMySQL::CheckMySQL() {}

bool CheckMySQL::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "mysql");

    // clang-format off
    settings.alias().add_key_to_settings()
      .add_string("hostname", sh::string_key(&defaults_.host, "localhost"),
        "MYSQL SERVER", "Default MySQL/MariaDB server to connect to.")
      .add_int("port", sh::int_key(&defaults_.port, 3306),
        "MYSQL PORT", "Default TCP port of the server.")
      .add_string("socket", sh::string_key(&defaults_.socket, ""),
        "MYSQL SOCKET", "Unix socket path (or Windows named pipe) to connect through instead of TCP.")
      .add_string("user", sh::string_key(&defaults_.user, ""),
        "MYSQL USER", "User used to authenticate.")
      .add_password("password", sh::string_key(&defaults_.password, ""),
        "MYSQL PASSWORD", "Password used to authenticate.")
      .add_string("database", sh::string_key(&defaults_.database, ""),
        "DATABASE", "Default database (schema) to connect to.")
      .add_string("defaults file", sh::string_key(&defaults_.defaults_file, ""),
        "DEFAULTS FILE", "my.cnf-style file whose [client] section supplies credentials, so passwords can be kept out of nsclient.ini.", true)
      .add_string("plugin dir", sh::string_key(&defaults_.plugin_dir, ""),
        "PLUGIN DIRECTORY", "Directory the connector loads client auth plugins from (needed for MySQL 8's caching_sha2_password when the connector's default is wrong).", true)
      .add_bool("tls", sh::bool_key(&defaults_.tls, false),
        "TLS", "Require TLS on the connection.", true)
      .add_int("timeout", sh::int_key(&defaults_.connect_timeout, 10),
        "CONNECTION TIMEOUT", "Connection timeout in seconds.", true)
      .add_int("query timeout", sh::int_key(&defaults_.query_timeout, 30),
        "QUERY TIMEOUT", "Query (read/write) timeout in seconds.", true)
      ;
    // clang-format on

    bool facts_server = false;
    bool facts_databases = false;
    // clang-format off
    settings.alias().add_key_to_settings("facts")
      .add_bool(mysql_facts::id_server, sh::bool_key(&facts_server, false),
        "MYSQL SERVER FACTS",
        "Collect the server record of the `mysql` fact set: the flavor (mysql, mariadb or percona, the same value check_mysql calls `flavor`), the "
        "version and version comment, the hostname and port the server reports about itself, its server id, its default character set and "
        "collation and the OS and architecture it was built for. Not its uptime or connections: those are monitoring, and they live in "
        "check_mysql. One connection and one query per facts round, made with the credentials configured in this section's parent (user, password "
        "or defaults file): a facts round has no request to take them from. Nothing is collected while this is off.")
      .add_bool(mysql_facts::id_databases, sh::bool_key(&facts_databases, false),
        "MYSQL DATABASES FACTS",
        "Collect the `mysql.databases` fact set: one record per database (schema) the configured user may see, system schemas included - its name "
        "(the record id), its default character set and collation. Not its size: that is monitoring. One query of information_schema.SCHEMATA "
        "per facts round, over the same connection as the server record. Nothing is collected while this is off.")
      ;
    // clang-format on

    settings.register_all();
    settings.notify();

    // Which parts of the set fetchFacts builds is configuration, so it is
    // re-read on every load, a reload included: the core drops a set a
    // producer stops returning, and that is what turning it off means.
    facts_server_.store(facts_server);
    facts_databases_.store(facts_databases);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("loading: ", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("loading: ");
    return false;
  }
  return true;
}

bool CheckMySQL::unloadModule() { return true; }

void CheckMySQL::fetchFacts(const nscapi::facts::request &, nscapi::facts::response &response) {
  mysql_facts::selection what;
  what.server = facts_server_.load();
  what.databases = facts_databases_.load();
  if (!what.any()) return;

  // Every round, whatever its reason: databases are created and dropped and
  // servers upgraded under a running agent, and a round costs one connection
  // and at most two queries. The connection is the configured one - a facts
  // round has no request to take a host= or a user= from.
  const mysql_client::connection_info info = defaults_;
  mysql_client::query_runner run;
  try {
    run = mysql_session::make_session_factory()(info);
  } catch (const mysql_client::mysql_exception &e) {
    // Named against the set rather than failing the round: the core keeps the
    // databases it already holds and reports why they are stale. A server
    // that is down for a minute must not blank the inventory.
    response.error(mysql_facts::set_mysql, "Failed to connect to MySQL server '" + info.display_target() + "': " + e.reason());
    return;
  } catch (const std::exception &e) {
    response.error(mysql_facts::set_mysql, "Failed to connect to MySQL server '" + info.display_target() + "': " + utf8::utf8_from_native(e.what()));
    return;
  }
  try {
    mysql_facts::publish(what, mysql_facts::gather(what, run), std::time(nullptr), response);
  } catch (const mysql_client::mysql_exception &e) {
    response.error(mysql_facts::set_mysql, "Query failed on MySQL server '" + info.display_target() + "': " + e.reason());
  } catch (const std::exception &e) {
    response.error(mysql_facts::set_mysql, "Failed to collect from MySQL server '" + info.display_target() + "': " + utf8::utf8_from_native(e.what()));
  }
}

void CheckMySQL::check_mysql(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mysql_command::check_with(defaults_, request, response, mysql_session::make_session_factory());
}

void CheckMySQL::check_mysql_query(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mysql_query_command::check_with(defaults_, request, response, mysql_session::make_session_factory());
}
