// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mysql_client.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace mysql_client {

std::size_t result::find_column(const std::string &col) const {
  for (std::size_t i = 0; i < columns.size(); i++) {
    if (columns[i] == col) return i;
  }
  throw mysql_exception("Column not found in result set: " + col);
}

std::string result::get_string(std::size_t row, const std::string &col) const { return get_string(row, find_column(col)); }

long long result::get_int(std::size_t row, const std::string &col) const { return get_int(row, find_column(col)); }

bool result::is_null(std::size_t row, const std::string &col) const { return rows[row][find_column(col)].null; }

std::string result::get_string(std::size_t row, std::size_t col) const { return rows[row][col].text; }

long long result::get_int(std::size_t row, std::size_t col) const {
  const cell &c = rows[row][col];
  if (c.null) return 0;
  char *end = nullptr;
  const long long value = std::strtoll(c.text.c_str(), &end, 10);
  // Numeric text like "99.2" parses better as a double. llround, not +0.5:
  // the latter rounds -99.6 toward zero (-99) instead of to nearest (-100).
  if (end != nullptr && *end == '.') return std::llround(std::strtod(c.text.c_str(), nullptr));
  return value;
}

std::string derive_flavor(const std::string &version, const std::string &version_comment) {
  std::string haystack = version + " " + version_comment;
  std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (haystack.find("mariadb") != std::string::npos) return "mariadb";
  if (haystack.find("percona") != std::string::npos) return "percona";
  return "mysql";
}

}  // namespace mysql_client
