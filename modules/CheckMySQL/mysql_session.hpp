// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include "mysql_client.hpp"

// The libmariadb-backed session. Only this pair of files includes the
// connector headers; everything else works against mysql_client types.
namespace mysql_session {

// RAII connection (MYSQL*). Speaks the native protocol to MySQL, MariaDB,
// Percona and other compatible servers via MariaDB Connector/C.
class session {
 public:
  explicit session(mysql_client::connection_info info);
  ~session();
  session(const session &) = delete;
  session &operator=(const session &) = delete;

  // mysql_real_connect with the configured timeouts/TLS/defaults-file; throws
  // mysql_client::mysql_exception with the server/client error on failure.
  void connect();
  // One statement per call; throws mysql_client::mysql_exception. Statements
  // that produce no result set return a result with no columns.
  mysql_client::result execute(const std::string &sql);

 private:
  mysql_client::connection_info info_;
  void *mysql_ = nullptr;  // MYSQL*
};

// The production session_factory: connects a session (throwing on failure)
// and returns a runner bound to it. The connection lives as long as the
// returned runner.
mysql_client::session_factory make_session_factory();

}  // namespace mysql_session
