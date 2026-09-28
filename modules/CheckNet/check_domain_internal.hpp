// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <stdexcept>
#include <string>

#include "check_http_internal.hpp"

namespace check_net {
namespace domain {
namespace internal {

inline check_http_internal::parsed_url https_url(const std::string &url) {
  check_http_internal::parsed_url parsed;
  if (url.find_first_of("\r\n\t #\\") != std::string::npos || url.find('@') != std::string::npos || !check_http_internal::parse_url(url, parsed) ||
      parsed.protocol != "https")
    throw std::runtime_error("RDAP requires an HTTPS URL without credentials, whitespace or fragment");
  return parsed;
}

// RFC 3986 section 5.2.4. Only literal path segments are normalized;
// percent-encoded dots and query data retain their original meaning.
inline std::string remove_dot_segments(std::string path) {
  std::string output;
  while (!path.empty()) {
    if (path.compare(0, 3, "../") == 0)
      path.erase(0, 3);
    else if (path.compare(0, 2, "./") == 0)
      path.erase(0, 2);
    else if (path.compare(0, 3, "/./") == 0)
      path.erase(0, 2);
    else if (path == "/.")
      path = "/";
    else if (path.compare(0, 4, "/../") == 0 || path == "/..") {
      path.replace(0, 3, "");
      if (path.empty()) path = "/";
      const auto slash = output.rfind('/');
      output.erase(slash == std::string::npos ? 0 : slash);
    } else if (path == "." || path == "..")
      path.clear();
    else {
      const auto end = path.find('/', path.front() == '/' ? 1 : 0);
      output += path.substr(0, end);
      path.erase(0, end == std::string::npos ? path.size() : end);
    }
  }
  return output;
}

inline std::string normalize_url_path(std::string url) {
  const auto authority_end = url.find_first_of("/?#", url.find("://") + 3);
  if (authority_end != std::string::npos && url[authority_end] == '?') url.insert(authority_end, "/");
  const auto parsed = https_url(url);
  const auto path_start = url.find('/', url.find("://") + 3);
  if (path_start == std::string::npos) return url;
  const auto query = parsed.path.find('?');
  return url.substr(0, path_start) + remove_dot_segments(parsed.path.substr(0, query)) + (query == std::string::npos ? "" : parsed.path.substr(query));
}

inline std::string redirected_url(const std::string &base, const std::string &target) {
  if (target.empty()) throw std::runtime_error("RDAP redirect has no Location");
  const auto colon = target.find(':');
  const auto path_start = target.find_first_of("/?#");
  if (colon != std::string::npos && (path_start == std::string::npos || colon < path_start)) return normalize_url_path(target);
  if (target.compare(0, 2, "//") == 0) return normalize_url_path("https:" + target);
  const auto u = https_url(base);
  const auto origin = "https://" + check_http_internal::host_header_value(u.host) + ":" + u.port;
  if (target.front() == '/') return normalize_url_path(origin + target);
  const auto path = u.path.substr(0, u.path.find('?'));
  if (target.front() == '?') return origin + path + target;
  return normalize_url_path(origin + path.substr(0, path.rfind('/') + 1) + target);
}
}  // namespace internal
}  // namespace domain
}  // namespace check_net
