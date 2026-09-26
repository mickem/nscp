// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckMSSQL.h"

#include <ctime>
#include <nscapi/macros.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

#include "check_mssql.hpp"
#include "check_mssql_backup.hpp"
#include "check_mssql_databases.hpp"
#include "check_mssql_jobs.hpp"
#include "check_mssql_query.hpp"
#include "mssql_facts.hpp"

namespace sh = nscapi::settings_helper;

CheckMSSQL::CheckMSSQL() {}

bool CheckMSSQL::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "mssql");

    // clang-format off
    settings.alias().add_key_to_settings()
      .add_string("hostname", sh::string_key(&defaults_.server, "localhost"),
        "SQL SERVER", "Default SQL Server to connect to: host, host\\INSTANCE or host,port.")
      .add_string("driver", sh::string_key(&defaults_.driver, ""),
        "ODBC DRIVER", "ODBC driver used to connect; leave empty to auto-detect the newest installed SQL Server driver.")
      .add_string("user", sh::string_key(&defaults_.user, ""),
        "SQL USER", "SQL login used to authenticate; leave user and password empty to use Windows integrated authentication.")
      .add_password("password", sh::string_key(&defaults_.password, ""),
        "SQL PASSWORD", "Password for the SQL login.")
      .add_string("database", sh::string_key(&defaults_.database, ""),
        "DATABASE", "Default database (initial catalog) to connect to.")
      .add_string("connection string", sh::string_key(&defaults_.raw_connection_string, ""),
        "CONNECTION STRING", "Raw ODBC connection string; overrides all other connection settings.", true)
      .add_int("timeout", sh::int_key(&defaults_.login_timeout, 10),
        "LOGIN TIMEOUT", "Connection (login) timeout in seconds.", true)
      .add_int("query timeout", sh::int_key(&defaults_.query_timeout, 30),
        "QUERY TIMEOUT", "Query timeout in seconds.", true)
      ;
    // clang-format on

    bool facts_server = false;
    bool facts_databases = false;
    // clang-format off
    settings.alias().add_key_to_settings("facts")
      .add_bool(mssql_facts::id_server, sh::bool_key(&facts_server, false),
        "MSSQL SERVER FACTS",
        "Collect the server record of the `mssql` fact set: the instance name (the same value check_mssql calls `server_name`), the machine and "
        "instance it is, the version, patch level, update level and edition, the engine edition, the server collation, the authentication mode "
        "and whether it is clustered or has Always On enabled. Not its uptime: that is monitoring, and it lives in check_mssql. One connection "
        "and one SERVERPROPERTY query per facts round, made with the connection configured in this section's parent (Windows authentication, "
        "or the user and password): a facts round has no request to take them from. Nothing is collected while this is off.")
      .add_bool(mssql_facts::id_databases, sh::bool_key(&facts_databases, false),
        "MSSQL DATABASES FACTS",
        "Collect the `mssql.databases` fact set: one record per database the login may see, system databases included - its name (the record id, "
        "the same value check_mssql_databases calls `name`), its recovery model, collation, compatibility level, creation date and whether it is "
        "read-only. Not its state or size: those are monitoring, and they live in check_mssql_databases. One query of sys.databases per facts "
        "round, over the same connection as the server record. Nothing is collected while this is off.")
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

bool CheckMSSQL::unloadModule() { return true; }

void CheckMSSQL::fetchFacts(const nscapi::facts::request &, nscapi::facts::response &response) {
  mssql_facts::selection what;
  what.server = facts_server_.load();
  what.databases = facts_databases_.load();
  if (!what.any()) return;

  // Every round, whatever its reason: databases are created and dropped and
  // instances patched under a running agent, and a round costs one connection
  // and at most two queries. The connection is the configured one - a facts
  // round has no request to take a server= or a user= from.
  const mssql_odbc::connection_info info = defaults_;
  try {
    mssql_odbc::session session(info);
    try {
      session.connect();
    } catch (const mssql_odbc::odbc_exception &e) {
      // Named against the set rather than failing the round: the core keeps
      // the databases it already holds and reports why they are stale. An
      // instance that is down for a minute must not blank the inventory.
      response.error(mssql_facts::set_mssql, "Failed to connect to SQL Server '" + info.server + "': " + e.reason());
      return;
    }
    try {
      const mssql_facts::snapshot snap = mssql_facts::gather(what, [&session](const std::string &sql) { return session.execute(sql); });
      mssql_facts::publish(what, snap, std::time(nullptr), response);
    } catch (const mssql_odbc::odbc_exception &e) {
      response.error(mssql_facts::set_mssql, "Query failed on SQL Server '" + info.server + "': " + e.reason());
    }
  } catch (const std::exception &e) {
    // Session construction (handle allocation), or something a query threw
    // that is not an ODBC error.
    response.error(mssql_facts::set_mssql, "Failed to collect from SQL Server '" + info.server + "': " + utf8::utf8_from_native(e.what()));
  }
}

void CheckMSSQL::check_mssql(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mssql_command::check(defaults_, request, response);
}

void CheckMSSQL::check_mssql_query(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mssql_query_command::check(defaults_, request, response);
}

void CheckMSSQL::check_mssql_databases(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mssql_databases_command::check(defaults_, request, response);
}

void CheckMSSQL::check_mssql_backup(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mssql_backup_command::check(defaults_, request, response);
}

void CheckMSSQL::check_mssql_jobs(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mssql_jobs_command::check(defaults_, request, response);
}
