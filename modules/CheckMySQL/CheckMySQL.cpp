// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckMySQL.h"

#include <nscapi/macros.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

#include "check_mysql.hpp"
#include "check_mysql_query.hpp"
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

bool CheckMySQL::unloadModule() { return true; }

void CheckMySQL::check_mysql(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mysql_command::check_with(defaults_, request, response, mysql_session::make_session_factory());
}

void CheckMySQL::check_mysql_query(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_mysql_query_command::check_with(defaults_, request, response, mysql_session::make_session_factory());
}
