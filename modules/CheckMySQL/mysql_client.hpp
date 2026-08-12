// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <exception>
#include <functional>
#include <string>
#include <vector>

// Client-side types shared by all CheckMySQL commands. Deliberately free of
// any libmariadb dependency: the checks and the unit tests consume these
// types plus an injectable session factory, and only mysql_session.cpp (not
// compiled into the test binary) touches the connector.
namespace mysql_client {

class mysql_exception : public std::exception {
  std::string message_;

 public:
  explicit mysql_exception(std::string message) : message_(std::move(message)) {}
  const char *what() const noexcept override { return message_.c_str(); }
  std::string reason() const { return message_; }
};

struct connection_info {
  std::string host = "localhost";  // TCP host (ignored when socket is set)
  int port = 3306;                 // TCP port
  std::string socket;              // unix socket path / Windows named pipe; wins over host when set
  std::string database;            // optional default schema
  std::string user;                // empty => the connector's default (current OS user)
  std::string password;
  std::string defaults_file;  // my.cnf-style option file read for [client] credentials
  std::string plugin_dir;     // client auth-plugin directory (e.g. for MySQL 8's caching_sha2_password)
  bool tls = false;           // require TLS on the connection
  int connect_timeout = 10;   // seconds
  int query_timeout = 30;     // seconds (read/write timeout per query)

  // Human-readable connection target for error messages ("host:port" or the socket path).
  std::string display_target() const { return socket.empty() ? host + ":" + std::to_string(port) : socket; }
};

struct cell {
  std::string text;
  bool null = false;
};

// A fully materialized result set. Check result sets are small, and
// materializing lets check_mysql_query get columns and rows from one execute.
struct result {
  std::vector<std::string> columns;
  std::vector<std::vector<cell>> rows;

  std::size_t find_column(const std::string &col) const;  // throws mysql_exception if absent
  std::string get_string(std::size_t row, const std::string &col) const;
  long long get_int(std::size_t row, const std::string &col) const;
  bool is_null(std::size_t row, const std::string &col) const;
  std::string get_string(std::size_t row, std::size_t col) const;
  long long get_int(std::size_t row, std::size_t col) const;
};

// Executes one SQL statement against a connected session; throws
// mysql_exception on query failure.
typedef std::function<result(const std::string &sql)> query_runner;

// Called once per check with the final (defaults + options) connection info;
// returns a runner bound to a live connection, or throws mysql_exception when
// the connection cannot be established. Injectable for unit tests.
typedef std::function<query_runner(const connection_info &info)> session_factory;

// Classify a server as mysql / mariadb / percona from its version string and
// version_comment ("10.11.14-MariaDB-ubu2404", "Percona Server (GPL), ...").
std::string derive_flavor(const std::string &version, const std::string &version_comment);

}  // namespace mysql_client
