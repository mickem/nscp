// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckMSSQL.h"

#include <nscapi/macros.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

#include "check_mssql.hpp"
#include "check_mssql_backup.hpp"
#include "check_mssql_databases.hpp"
#include "check_mssql_jobs.hpp"
#include "check_mssql_query.hpp"

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

    settings.register_all();
    settings.notify();
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
