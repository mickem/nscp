// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <exception>
#include <string>
#include <vector>

namespace mssql_odbc {

class odbc_exception : public std::exception {
  std::string message_;

 public:
  explicit odbc_exception(std::string message) : message_(std::move(message)) {}
  const char *what() const noexcept override { return message_.c_str(); }
  std::string reason() const { return message_; }
};

// Brace-quote a connection-string value if it contains characters the ODBC
// driver manager would otherwise misparse (';', '{', '}' or leading space).
std::string quote_connection_value(const std::string &value);

struct connection_info {
  std::string server = "localhost";  // host, host\INSTANCE or host,port
  std::string database;              // optional initial catalog
  std::string user;                  // user+password empty => Trusted_Connection=yes
  std::string password;
  std::string driver;                 // empty => auto-detect via pick_driver()
  std::string raw_connection_string;  // full override; wins over everything else
  bool trust_server_cert = true;      // TrustServerCertificate=yes (modern drivers only)
  std::string encrypt;                // ""|"yes"|"no"; emitted only if set (modern drivers only)
  int login_timeout = 10;             // seconds, SQL_ATTR_LOGIN_TIMEOUT
  int query_timeout = 30;             // seconds, SQL_ATTR_QUERY_TIMEOUT

  std::string to_connection_string(const std::string &resolved_driver) const;
};

struct cell {
  std::string text;
  bool null = false;
};

// A fully materialized result set. Check result sets are small, and
// materializing lets check_mssql_query get columns and rows from one execute.
struct result {
  std::vector<std::string> columns;
  std::vector<std::vector<cell>> rows;

  std::size_t find_column(const std::string &col) const;  // throws odbc_exception if absent
  std::string get_string(std::size_t row, const std::string &col) const;
  long long get_int(std::size_t row, const std::string &col) const;
  bool is_null(std::size_t row, const std::string &col) const;
  // Positional accessors for result sets whose column names are unreliable
  // (DBCC output is localized on non-English servers).
  std::string get_string(std::size_t row, std::size_t col) const;
  long long get_int(std::size_t row, std::size_t col) const;
};

// RAII ODBC session (HENV + HDBC). Uses the wide (W) ODBC API throughout and
// converts to UTF-8 at the boundary.
class session {
 public:
  explicit session(connection_info info);
  ~session();
  session(const session &) = delete;
  session &operator=(const session &) = delete;

  // SQLDriverConnectW with SQL_ATTR_LOGIN_TIMEOUT; throws odbc_exception with
  // the full SQLGetDiagRecW chain ("[state/native] message, ...") on failure.
  void connect();
  // One HSTMT per call with SQL_ATTR_QUERY_TIMEOUT; throws odbc_exception.
  result execute(const std::string &sql);

  static std::vector<std::string> installed_drivers();
  static std::string pick_driver(const std::vector<std::string> &installed);

 private:
  connection_info info_;
  void *env_ = nullptr;  // SQLHENV
  void *dbc_ = nullptr;  // SQLHDBC
  bool connected_ = false;
};

}  // namespace mssql_odbc
