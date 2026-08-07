// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "odbc_query.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <str/utf8.hpp>
#include <vector>

#ifdef WIN32

// clang-format off
// windows.h must precede sql.h: sqltypes.h builds on the Win32 typedefs and
// does not compile without them. Keep clang-format from alphabetising these.
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
// clang-format on

namespace mssql_odbc {

namespace {

std::string to_utf8(const SQLWCHAR *str, std::size_t len) { return utf8::cvt<std::string>(std::wstring(reinterpret_cast<const wchar_t *>(str), len)); }

// ODBC length-in-characters outputs (SQLGetDiagRecW, SQLDescribeColW,
// SQLDriversW) report the length *available*, not the length written: when the
// buffer is too small the driver truncates and still reports the full length.
// Never trust it - clamp to the real capacity and stop at the first NUL the
// driver actually wrote, so neither a truncating nor an under-filling driver
// can make us read past the buffer.
std::size_t safe_len(const SQLWCHAR *buffer, const std::size_t capacity, const SQLSMALLINT reported) {
  std::size_t len = reported < 0 ? 0 : static_cast<std::size_t>(reported);
  if (len > capacity) len = capacity;
  return static_cast<std::size_t>(std::find(buffer, buffer + len, 0) - buffer);
}

std::string to_utf8_safe(const std::vector<SQLWCHAR> &buffer, const SQLSMALLINT reported) {
  return to_utf8(buffer.data(), safe_len(buffer.data(), buffer.size(), reported));
}

// Collect the full diagnostic chain from a handle: "[state/native] message, ...".
std::string format_diag(SQLSMALLINT handle_type, SQLHANDLE handle) {
  // 1024 covers essentially every driver message; the two-call retry below
  // handles the rest (SQL Server messages reach 2047 characters and the driver
  // prepends its own "[Microsoft][ODBC Driver NN...]" prefixes).
  const std::size_t initial_chars = 1024;
  std::string message;
  for (SQLSMALLINT rec = 1;; rec++) {
    std::vector<SQLWCHAR> state(SQL_SQLSTATE_SIZE + 1, 0);
    std::vector<SQLWCHAR> text(initial_chars, 0);
    SQLINTEGER native = 0;
    SQLSMALLINT text_len = 0;
    if (!SQL_SUCCEEDED(SQLGetDiagRecW(handle_type, handle, rec, state.data(), &native, text.data(), static_cast<SQLSMALLINT>(text.size()), &text_len))) break;
    const std::size_t needed = text_len > 0 ? static_cast<std::size_t>(text_len) + 1 : 0;
    // Only retry when the required buffer length is still expressible in the
    // SQLSMALLINT length argument; otherwise keep (and clamp) what we have.
    if (needed > text.size() && needed <= static_cast<std::size_t>((std::numeric_limits<SQLSMALLINT>::max)())) {
      // Truncated: re-fetch the same record into a buffer sized from the
      // reported length. If the retry fails we still render the truncated text.
      std::vector<SQLWCHAR> full(needed, 0);
      SQLSMALLINT full_len = 0;
      if (SQL_SUCCEEDED(SQLGetDiagRecW(handle_type, handle, rec, state.data(), &native, full.data(), static_cast<SQLSMALLINT>(full.size()), &full_len))) {
        text.swap(full);
        text_len = full_len;
      }
    }
    if (!message.empty()) message += ", ";
    message += "[" + to_utf8_safe(state, SQL_SQLSTATE_SIZE) + "/" + std::to_string(native) + "] " + to_utf8_safe(text, text_len);
  }
  if (message.empty()) message = "unknown ODBC error";
  return message;
}

// RAII owner for an ODBC handle: frees on scope exit unless ownership has been
// handed over with release(). Keeps every error path leak-free.
struct handle_guard {
  SQLSMALLINT type;
  SQLHANDLE handle = SQL_NULL_HANDLE;
  explicit handle_guard(const SQLSMALLINT type) : type(type) {}
  ~handle_guard() {
    if (handle != SQL_NULL_HANDLE) SQLFreeHandle(type, handle);
  }
  handle_guard(const handle_guard &) = delete;
  handle_guard &operator=(const handle_guard &) = delete;
  bool alloc(SQLHANDLE parent) { return SQL_SUCCEEDED(SQLAllocHandle(type, parent, &handle)); }
  SQLHANDLE release() {
    SQLHANDLE released = handle;
    handle = SQL_NULL_HANDLE;
    return released;
  }
};

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
  // Both handles stay owned by the guards until the constructor can no longer
  // fail; only then is ownership released into the (raw, header-opaque)
  // members which the destructor frees.
  handle_guard env(SQL_HANDLE_ENV);
  if (!env.alloc(SQL_NULL_HANDLE)) throw odbc_exception("Failed to allocate ODBC environment handle");
  SQLSetEnvAttr(static_cast<SQLHENV>(env.handle), SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
  handle_guard dbc(SQL_HANDLE_DBC);
  if (!dbc.alloc(env.handle)) throw odbc_exception("Failed to allocate ODBC connection handle: " + format_diag(SQL_HANDLE_ENV, env.handle));
  env_ = env.release();
  dbc_ = dbc.release();
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
  // A batch yields one "result" per statement, and a DML statement without
  // SET NOCOUNT ON produces a row-count result with zero columns *before* the
  // SELECT the user cares about. Skip those with SQLMoreResults so
  // "UPDATE ...; SELECT ..." reports the SELECT instead of silently returning
  // nothing. Only the first column-bearing result set is read; res.columns
  // stays empty when the batch produced none at all, which the caller reports.
  SQLSMALLINT column_count = 0;
  for (;;) {
    if (!SQL_SUCCEEDED(SQLNumResultCols(stmt.handle, &column_count))) throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt.handle));
    if (column_count > 0) break;
    const SQLRETURN more = SQLMoreResults(stmt.handle);
    if (more == SQL_NO_DATA) return res;
    if (!SQL_SUCCEEDED(more)) throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt.handle));
  }
  for (SQLUSMALLINT col = 1; col <= column_count; col++) {
    // 256 characters holds any SQL Server identifier (sysname = 128) plus room
    // for a longer expression alias; anything beyond is clamped, never over-read.
    std::vector<SQLWCHAR> name(256, 0);
    SQLSMALLINT name_len = 0, data_type = 0, decimal_digits = 0, nullable = 0;
    SQLULEN column_size = 0;
    if (!SQL_SUCCEEDED(SQLDescribeColW(stmt.handle, col, name.data(), static_cast<SQLSMALLINT>(name.size()), &name_len, &data_type, &column_size,
                                       &decimal_digits, &nullable)))
      throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt.handle));
    res.columns.push_back(to_utf8_safe(name, name_len));
  }

  while (SQL_SUCCEEDED(ret = SQLFetch(stmt.handle))) {
    std::vector<cell> row;
    row.reserve(column_count);
    for (SQLUSMALLINT col = 1; col <= column_count; col++) row.push_back(fetch_cell(stmt.handle, col));
    res.rows.push_back(std::move(row));
  }
  if (ret != SQL_NO_DATA && !SQL_SUCCEEDED(ret)) throw odbc_exception(format_diag(SQL_HANDLE_STMT, stmt.handle));
  return res;
}

std::vector<std::string> session::installed_drivers() {
  std::vector<std::string> drivers;
  // Guarded: push_back can throw, and the environment must not leak if it does.
  handle_guard env(SQL_HANDLE_ENV);
  if (!env.alloc(SQL_NULL_HANDLE)) return drivers;
  SQLSetEnvAttr(static_cast<SQLHENV>(env.handle), SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
  std::vector<SQLWCHAR> description(256, 0), attributes(256, 0);
  SQLSMALLINT desc_len = 0, attr_len = 0;
  SQLUSMALLINT direction = SQL_FETCH_FIRST;
  while (SQL_SUCCEEDED(SQLDriversW(static_cast<SQLHENV>(env.handle), direction, description.data(), static_cast<SQLSMALLINT>(description.size()), &desc_len,
                                   attributes.data(), static_cast<SQLSMALLINT>(attributes.size()), &attr_len))) {
    drivers.push_back(to_utf8_safe(description, desc_len));
    direction = SQL_FETCH_NEXT;
  }
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

std::string result::get_string(std::size_t row, const std::string &col) const { return get_string(row, find_column(col)); }

long long result::get_int(std::size_t row, const std::string &col) const { return get_int(row, find_column(col)); }

std::string result::get_string(std::size_t row, std::size_t col) const { return rows[row][col].text; }

long long result::get_int(std::size_t row, std::size_t col) const {
  const cell &c = rows[row][col];
  if (c.null) return 0;
  char *end = nullptr;
  const long long value = std::strtoll(c.text.c_str(), &end, 10);
  // Numeric text like "99.2" (DBCC SQLPERF) parses better as a double.
  // llround, not +0.5: the latter rounds -99.6 toward zero (-99) instead of
  // to nearest (-100).
  if (end != nullptr && *end == '.') return std::llround(std::strtod(c.text.c_str(), nullptr));
  return value;
}

bool result::is_null(std::size_t row, const std::string &col) const { return rows[row][find_column(col)].null; }

}  // namespace mssql_odbc
