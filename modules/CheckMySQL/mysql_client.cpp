// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mysql_client.hpp"

#include <algorithm>
// std::tolower lives in <cctype>. libstdc++ drags it in through <algorithm>,
// MSVC's does not, so leaving it out builds on Linux and fails on Windows.
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace mysql_client {

std::string connection_override_refusal(const connection_info &configured, const connection_info &requested) {
  if (configured.allow_host_override) return "";

  std::vector<std::string> changed;
  // socket is settings-only, and when set it wins over host and port (see
  // mysql_session.cpp), so with one configured they cannot move anything.
  if (configured.socket.empty()) {
    if (requested.host != configured.host) changed.push_back("host");
    if (requested.port != configured.port) changed.push_back("port");
  }
  // Only the weakening direction: requiring TLS where the configuration does
  // not moves nothing.
  if (configured.tls && !requested.tls) changed.push_back("tls");
  if (changed.empty()) return "";

  const bool configured_credentials = !configured.user.empty() || !configured.password.empty() || !configured.defaults_file.empty();
  if (!configured_credentials) return "";

  // The request's own credentials: both named. With a defaults file
  // configured an empty value is not the request's own - the connector falls
  // back to the file's [client] value for it (an empty user or password is
  // passed as NULL).
  const bool own_credentials = requested.user_from_request && requested.password_from_request &&
                               (configured.defaults_file.empty() || (!requested.user.empty() && !requested.password.empty()));
  if (own_credentials) return "";

  std::string how;
  for (const std::string &option : changed) {
    if (!how.empty()) how += "/";
    how += option;
  }
  return "The connection uses the credentials configured in /settings/mysql, so the request cannot change " + how +
         ": that would send them to a server of the caller's choosing, or without the TLS the configuration requires. Supply user= and password= "
         "with the request, configure the server in /settings/mysql, or set 'allow host override = true' there to permit this.";
}

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
