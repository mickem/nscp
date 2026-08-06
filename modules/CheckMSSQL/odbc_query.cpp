// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "odbc_query.hpp"

#include <algorithm>
#include <cstdlib>
#include <str/utf8.hpp>

#ifdef WIN32

// windows.h must precede sql.h: sqltypes.h relies on the Win32 typedefs.
// The blank line keeps clang-format from sorting it below the sql headers.
#include <windows.h>

#include <sql.h>
#include <sqlext.h>

namespace mssql_odbc {

namespace {

std::string to_utf8(const SQLWCHAR *str, std::size_t len) { return utf8::cvt<std::string>(std::wstring(reinterpret_cast<const wchar_t *>(str), len)); }

// Collect the full diagnostic chain from a handle: "[state/native] message, ...".
std::string format_diag(SQLSMALLINT handle_type, SQLHANDLE handle) {
  std::string message;
  SQLWCHAR state[SQL_SQLSTATE_SIZE + 1];
  SQLWCHAR text[1024];
  SQLINTEGER native = 0;
  SQLSMALLINT text_len = 0;
  for (SQLSMALLINT rec = 1; SQL_SUCCEEDED(SQLGetDiagRecW(handle_type, handle, rec, state, &native, text, sizeof(text) / sizeof(SQLWCHAR), &text_len)); rec++) {
    if (!message.empty()) message += ", ";
    message += "[" + to_utf8(state, SQL_SQLSTATE_SIZE) + "/" + std::to_string(native) + "] " + to_utf8(text, text_len);
  }
  if (message.empty()) message = "unknown ODBC error";
  return message;
}

// RAII statement handle so error paths cannot leak the HSTMT.
struct statement {
  SQLHSTMT handle = SQL_NULL_HSTMT;
  explicit statement(SQLHDBC dbc) {
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle)))
      throw odbc_exception("Failed to allocate ODBC statement handle: " + format_diag(SQL_HANDLE_DBC, dbc));
  }
  ~statement() {
    if (handle != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, handle);
  }
  statement(const statement &) = delete;
  statement &operator=(const statement &) = delete;
};

cell fetch_cell(SQLHSTMT stmt, SQLUSMALLINT col) {
  cell c;
  std::wstring value;
  SQLWCHAR buffer[4096];
  SQLLEN indicator = 0;
  SQLRETURN ret;
  // Loop SQLGetData: long values are returned in chunks (SQL_SUCCESS_WITH_INFO
  // + truncation) until SQL_SUCCESS / SQL_NO_DATA.
  while ((ret = SQLGetData(stmt, col, SQL_C_WCHAR, buffer, sizeof(buffer), &indicator)) != SQL_NO_DATA) {
    if (!SQL_SUCCEEDED(ret)) throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt));
    if (indicator == SQL_NULL_DATA) {
      c.null = true;
      return c;
    }
    const std::size_t buffer_chars = sizeof(buffer) / sizeof(SQLWCHAR) - 1;  // minus NUL terminator
    std::size_t chars;
    if (indicator == SQL_NO_TOTAL || static_cast<std::size_t>(indicator) / sizeof(SQLWCHAR) > buffer_chars)
      chars = buffer_chars;
    else
      chars = static_cast<std::size_t>(indicator) / sizeof(SQLWCHAR);
    value.append(reinterpret_cast<const wchar_t *>(buffer), chars);
    if (ret == SQL_SUCCESS) break;
  }
  c.text = utf8::cvt<std::string>(value);
  return c;
}

}  // namespace

session::session(connection_info info) : info_(std::move(info)) {
  SQLHENV env = SQL_NULL_HENV;
  if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env))) throw odbc_exception("Failed to allocate ODBC environment handle");
  env_ = env;
  SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
  SQLHDBC dbc = SQL_NULL_HDBC;
  if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc))) {
    const std::string reason = format_diag(SQL_HANDLE_ENV, env);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    env_ = nullptr;
    throw odbc_exception("Failed to allocate ODBC connection handle: " + reason);
  }
  dbc_ = dbc;
}

session::~session() {
  if (dbc_ != nullptr) {
    if (connected_) SQLDisconnect(static_cast<SQLHDBC>(dbc_));
    SQLFreeHandle(SQL_HANDLE_DBC, static_cast<SQLHDBC>(dbc_));
  }
  if (env_ != nullptr) SQLFreeHandle(SQL_HANDLE_ENV, static_cast<SQLHENV>(env_));
}

void session::connect() {
  const auto dbc = static_cast<SQLHDBC>(dbc_);
  SQLSetConnectAttrW(dbc, SQL_ATTR_LOGIN_TIMEOUT, reinterpret_cast<SQLPOINTER>(static_cast<SQLULEN>(info_.login_timeout)), 0);

  std::string driver = info_.driver;
  if (driver.empty() && info_.raw_connection_string.empty()) driver = pick_driver(installed_drivers());
  const std::wstring connection_string = utf8::cvt<std::wstring>(info_.to_connection_string(driver));

  SQLSMALLINT out_len = 0;
  const SQLRETURN ret = SQLDriverConnectW(dbc, nullptr, const_cast<SQLWCHAR *>(reinterpret_cast<const SQLWCHAR *>(connection_string.c_str())), SQL_NTS, nullptr,
                                          0, &out_len, SQL_DRIVER_NOPROMPT);
  if (!SQL_SUCCEEDED(ret)) throw odbc_exception(format_diag(SQL_HANDLE_DBC, dbc));
  connected_ = true;
}

result session::execute(const std::string &sql) {
  const auto dbc = static_cast<SQLHDBC>(dbc_);
  statement stmt(dbc);
  SQLSetStmtAttrW(stmt.handle, SQL_ATTR_QUERY_TIMEOUT, reinterpret_cast<SQLPOINTER>(static_cast<SQLULEN>(info_.query_timeout)), 0);

  const std::wstring wsql = utf8::cvt<std::wstring>(sql);
  SQLRETURN ret = SQLExecDirectW(stmt.handle, const_cast<SQLWCHAR *>(reinterpret_cast<const SQLWCHAR *>(wsql.c_str())), SQL_NTS);
  if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA) throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt.handle));

  result res;
  SQLSMALLINT column_count = 0;
  if (!SQL_SUCCEEDED(SQLNumResultCols(stmt.handle, &column_count))) throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt.handle));
  for (SQLUSMALLINT col = 1; col <= column_count; col++) {
    SQLWCHAR name[256];
    SQLSMALLINT name_len = 0, data_type = 0, decimal_digits = 0, nullable = 0;
    SQLULEN column_size = 0;
    if (!SQL_SUCCEEDED(
            SQLDescribeColW(stmt.handle, col, name, sizeof(name) / sizeof(SQLWCHAR), &name_len, &data_type, &column_size, &decimal_digits, &nullable)))
      throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt.handle));
    res.columns.push_back(to_utf8(name, name_len));
  }

  while (column_count > 0 && SQL_SUCCEEDED(ret = SQLFetch(stmt.handle))) {
    std::vector<cell> row;
    row.reserve(column_count);
    for (SQLUSMALLINT col = 1; col <= column_count; col++) row.push_back(fetch_cell(stmt.handle, col));
    res.rows.push_back(std::move(row));
  }
  if (column_count > 0 && ret != SQL_NO_DATA && !SQL_SUCCEEDED(ret)) throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt.handle));
  return res;
}

std::vector<std::string> session::installed_drivers() {
  std::vector<std::string> drivers;
  SQLHENV env = SQL_NULL_HENV;
  if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env))) return drivers;
  SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
  SQLWCHAR description[256], attributes[256];
  SQLSMALLINT desc_len = 0, attr_len = 0;
  SQLUSMALLINT direction = SQL_FETCH_FIRST;
  while (SQL_SUCCEEDED(SQLDriversW(env, direction, description, sizeof(description) / sizeof(SQLWCHAR), &desc_len, attributes,
                                   sizeof(attributes) / sizeof(SQLWCHAR), &attr_len))) {
    drivers.push_back(to_utf8(description, desc_len));
    direction = SQL_FETCH_NEXT;
  }
  SQLFreeHandle(SQL_HANDLE_ENV, env);
  return drivers;
}

}  // namespace mssql_odbc

#endif  // WIN32

namespace mssql_odbc {

std::string quote_connection_value(const std::string &value) {
  if (value.find_first_of(";{}") == std::string::npos && (value.empty() || value.front() != ' ')) return value;
  std::string quoted = "{";
  for (const char c : value) {
    quoted += c;
    if (c == '}') quoted += '}';  // '}' is escaped by doubling inside a braced value
  }
  quoted += "}";
  return quoted;
}

std::string connection_info::to_connection_string(const std::string &resolved_driver) const {
  if (!raw_connection_string.empty()) return raw_connection_string;
  std::string cs = "Driver={" + resolved_driver + "};Server=" + quote_connection_value(server) + ";";
  if (!database.empty()) cs += "Database=" + quote_connection_value(database) + ";";
  if (user.empty() && password.empty())
    cs += "Trusted_Connection=yes;";
  else
    cs += "UID=" + quote_connection_value(user) + ";PWD=" + quote_connection_value(password) + ";";
  // Encrypt/TrustServerCertificate are only understood by the modern
  // "ODBC Driver NN for SQL Server" drivers; Driver 18 defaults Encrypt=yes,
  // which rejects the typical self-signed instance certificate.
  if (resolved_driver.compare(0, 12, "ODBC Driver ") == 0) {
    if (!encrypt.empty()) cs += "Encrypt=" + encrypt + ";";
    if (trust_server_cert) cs += "TrustServerCertificate=yes;";
  }
  cs += "APP=NSClient++;";
  return cs;
}

std::string session::pick_driver(const std::vector<std::string> &installed) {
  static const char *preferred[] = {"ODBC Driver 18 for SQL Server", "ODBC Driver 17 for SQL Server", "ODBC Driver 13 for SQL Server",
                                    "ODBC Driver 11 for SQL Server", "SQL Server Native Client 11.0", "SQL Server"};
  for (const char *candidate : preferred) {
    if (std::find(installed.begin(), installed.end(), candidate) != installed.end()) return candidate;
  }
  // The legacy sqlsrv32 driver ships with every Windows, so this is unreachable
  // in practice; keep it as a deterministic fallback.
  return "SQL Server";
}

std::size_t result::find_column(const std::string &col) const {
  for (std::size_t i = 0; i < columns.size(); i++) {
    if (columns[i] == col) return i;
  }
  throw odbc_exception("Column not found in result set: " + col);
}

std::string result::get_string(std::size_t row, const std::string &col) const { return rows[row][find_column(col)].text; }

long long result::get_int(std::size_t row, const std::string &col) const {
  const cell &c = rows[row][find_column(col)];
  if (c.null) return 0;
  char *end = nullptr;
  const long long value = std::strtoll(c.text.c_str(), &end, 10);
  // Numeric text like "99.2" (DBCC SQLPERF) parses better as a double.
  if (end != nullptr && *end == '.') return static_cast<long long>(std::strtod(c.text.c_str(), nullptr) + 0.5);
  return value;
}

bool result::is_null(std::size_t row, const std::string &col) const { return rows[row][find_column(col)].null; }

}  // namespace mssql_odbc
