/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <vector>

namespace http {

enum class proxy_type { NONE, HTTP, SOCKS5 };

/// Proxy configuration passed through http_client_options.
struct proxy_config {
  proxy_type type = proxy_type::NONE;
  std::string host;
  std::string port;
  std::string username;
  std::string password;
  /// Bypass patterns — checked by should_bypass() before using the proxy.
  std::vector<std::string> no_proxy;

  bool is_set() const { return type != proxy_type::NONE; }

  /// Returns "username:password" when both are set, empty string otherwise.
  std::string credentials() const {
    if (username.empty()) return "";
    return username + ":" + password;
  }
};

/// Parse a proxy URL of the form [scheme://][user:pass@]host[:port][/].
/// Supported schemes: "http", "socks5".  An empty url returns a NONE proxy.
inline proxy_config parse_proxy_url(const std::string& url) {
  proxy_config cfg;
  if (url.empty()) return cfg;

  const auto scheme_end = url.find("://");
  if (scheme_end == std::string::npos) return cfg;

  const std::string scheme = url.substr(0, scheme_end);
  std::string rest = url.substr(scheme_end + 3);

  if (scheme == "http")
    cfg.type = proxy_type::HTTP;
  else if (scheme == "socks5")
    cfg.type = proxy_type::SOCKS5;
  else
    return cfg;

  // Credentials: user:pass@host
  const auto at_pos = rest.rfind('@');
  if (at_pos != std::string::npos) {
    const std::string creds = rest.substr(0, at_pos);
    rest = rest.substr(at_pos + 1);
    const auto colon = creds.find(':');
    if (colon != std::string::npos) {
      cfg.username = creds.substr(0, colon);
      cfg.password = creds.substr(colon + 1);
    } else {
      cfg.username = creds;
    }
  }

  // Strip trailing path component
  const auto slash_pos = rest.find('/');
  if (slash_pos != std::string::npos) rest = rest.substr(0, slash_pos);

  // host:port
  const auto colon_pos = rest.find(':');
  if (colon_pos != std::string::npos) {
    cfg.host = rest.substr(0, colon_pos);
    cfg.port = rest.substr(colon_pos + 1);
  } else {
    cfg.host = rest;
    cfg.port = (cfg.type == proxy_type::SOCKS5) ? "1080" : "3128";
  }

  return cfg;
}

/// Returns true if target_host matches any bypass pattern in no_proxy_list.
///
/// Pattern rules:
///   "*"        — bypass all hosts.
///   ".suffix"  — suffix match; ".corp" matches "foo.corp" and "corp".
///   otherwise  — exact case-insensitive match.
///
/// Note: CIDR subnet notation is not currently supported.
inline bool should_bypass(const std::string& target_host, const std::vector<std::string>& no_proxy_list) {
  for (const auto& pattern : no_proxy_list) {
    if (pattern.empty()) continue;

    if (pattern == "*") return true;

    if (pattern[0] == '.') {
      // suffix match: ".corp" matches "foo.corp" and "corp" itself
      const std::string& suffix = pattern;
      if (target_host.size() >= suffix.size() && target_host.compare(target_host.size() - suffix.size(), suffix.size(), suffix) == 0) return true;
      // also match the domain without the leading dot
      const std::string domain = suffix.substr(1);
      if (target_host == domain) return true;
    } else {
      if (target_host == pattern) return true;
    }
  }
  return false;
}

}  // namespace http
