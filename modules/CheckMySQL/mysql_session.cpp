// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mysql_session.hpp"

#include <memory>
#include <mysql.h>

namespace mysql_session {

namespace {
MYSQL *handle(void *p) { return static_cast<MYSQL *>(p); }

std::string client_error(MYSQL *mysql) {
  const char *msg = mysql_error(mysql);
  return msg != nullptr && *msg != '\0' ? std::string(msg) : "unknown MySQL client error";
}
}  // namespace

session::session(mysql_client::connection_info info) : info_(std::move(info)) {
  mysql_ = mysql_init(nullptr);
  if (mysql_ == nullptr) throw mysql_client::mysql_exception("mysql_init failed (out of memory)");
}

session::~session() {
  if (mysql_ != nullptr) mysql_close(handle(mysql_));
}

void session::connect() {
  MYSQL *mysql = handle(mysql_);

  const unsigned int connect_timeout = info_.connect_timeout > 0 ? static_cast<unsigned int>(info_.connect_timeout) : 10;
  const unsigned int query_timeout = info_.query_timeout > 0 ? static_cast<unsigned int>(info_.query_timeout) : 30;
  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &query_timeout);
  mysql_options(mysql, MYSQL_OPT_WRITE_TIMEOUT, &query_timeout);
  if (!info_.defaults_file.empty()) {
    // Credentials from a my.cnf-style file ([client] group) so passwords can
    // live in a permission-protected file instead of nsclient.ini.
    mysql_options(mysql, MYSQL_READ_DEFAULT_FILE, info_.defaults_file.c_str());
    mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, "client");
  }
  if (info_.socket.empty()) {
    // The connector silently turns host "localhost" into a default-socket
    // connection; a monitoring check's transport must be explicit instead:
    // TCP unless a socket/pipe was requested.
    unsigned int protocol = MYSQL_PROTOCOL_TCP;
    mysql_options(mysql, MYSQL_OPT_PROTOCOL, &protocol);
  }
  if (!info_.plugin_dir.empty()) {
    // Where the connector loads client auth plugins from (MySQL 8 accounts
    // default to caching_sha2_password, which is such a plugin). Only needed
    // when the connector's compiled-in default is wrong for this install.
    mysql_options(mysql, MYSQL_PLUGIN_DIR, info_.plugin_dir.c_str());
  }
  if (info_.tls) {
    my_bool enforce = 1;
    mysql_options(mysql, MYSQL_OPT_SSL_ENFORCE, &enforce);
  }

  const char *host = info_.socket.empty() ? (info_.host.empty() ? nullptr : info_.host.c_str()) : nullptr;
  const char *user = info_.user.empty() ? nullptr : info_.user.c_str();
  const char *password = info_.password.empty() ? nullptr : info_.password.c_str();
  const char *database = info_.database.empty() ? nullptr : info_.database.c_str();
  const char *socket = info_.socket.empty() ? nullptr : info_.socket.c_str();
  const unsigned int port = info_.socket.empty() ? static_cast<unsigned int>(info_.port) : 0;

  if (mysql_real_connect(mysql, host, user, password, database, port, socket, 0) == nullptr) {
    throw mysql_client::mysql_exception(client_error(mysql));
  }
}

mysql_client::result session::execute(const std::string &sql) {
  MYSQL *mysql = handle(mysql_);
  if (mysql_real_query(mysql, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
    throw mysql_client::mysql_exception(client_error(mysql));
  }

  mysql_client::result res;
  MYSQL_RES *store = mysql_store_result(mysql);
  if (store == nullptr) {
    // A statement without a result set (e.g. SET) yields field_count 0;
    // anything else is a real retrieval error.
    if (mysql_field_count(mysql) == 0) return res;
    throw mysql_client::mysql_exception(client_error(mysql));
  }

  const unsigned int num_fields = mysql_num_fields(store);
  const MYSQL_FIELD *fields = mysql_fetch_fields(store);
  for (unsigned int i = 0; i < num_fields; i++) {
    res.columns.push_back(fields[i].name != nullptr ? fields[i].name : "");
  }
  for (MYSQL_ROW row = mysql_fetch_row(store); row != nullptr; row = mysql_fetch_row(store)) {
    const unsigned long *lengths = mysql_fetch_lengths(store);
    std::vector<mysql_client::cell> cells;
    cells.reserve(num_fields);
    for (unsigned int i = 0; i < num_fields; i++) {
      mysql_client::cell c;
      if (row[i] == nullptr) {
        c.null = true;
      } else {
        c.text.assign(row[i], lengths != nullptr ? lengths[i] : 0);
      }
      cells.push_back(std::move(c));
    }
    res.rows.push_back(std::move(cells));
  }
  mysql_free_result(store);
  return res;
}

mysql_client::session_factory make_session_factory() {
  return [](const mysql_client::connection_info &info) -> mysql_client::query_runner {
    auto live = std::make_shared<session>(info);
    live->connect();
    return [live](const std::string &sql) { return live->execute(sql); };
  };
}

}  // namespace mysql_session
