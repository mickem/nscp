// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/case_conv.hpp>
#include <string>

namespace check_net {
namespace check_http_internal {

struct parsed_url {
  std::string protocol;
  std::string host;
  std::string port;
  std::string path;
};

// True when the string is a usable TCP port: all digits, 1-65535. parse_url
// validates the port with this so callers can std::stoll it without a try
// block (stoll would otherwise throw on ":" with nothing after it, and
// silently parse "8o80" as 8).
inline bool valid_port(const std::string &port) {
  if (port.empty() || port.size() > 5) return false;
  for (const char c : port)
    if (c < '0' || c > '9') return false;
  const int value = std::stoi(port);
  return value >= 1 && value <= 65535;
}

// Minimal URL parser for http(s)://host[:port]/path. Returns false on failure.
inline bool parse_url(const std::string &url, parsed_url &out) {
  std::string s = url;
  const auto sep = s.find("://");
  if (sep == std::string::npos) return false;
  out.protocol = s.substr(0, sep);
  boost::algorithm::to_lower(out.protocol);
  if (out.protocol != "http" && out.protocol != "https") return false;
  s = s.substr(sep + 3);
  const auto slash = s.find('/');
  std::string host_port;
  if (slash == std::string::npos) {
    host_port = s;
    out.path = "/";
  } else {
    host_port = s.substr(0, slash);
    out.path = s.substr(slash);
  }
  // An IPv6 literal is bracketed (RFC 3986) precisely because it is full of
  // colons: "[::1]:8080". Strip the brackets and only look for a port
  // separator after them, otherwise the first colon of the address is mistaken
  // for the port separator.
  if (!host_port.empty() && host_port[0] == '[') {
    const auto close = host_port.find(']');
    if (close == std::string::npos) return false;
    out.host = host_port.substr(1, close - 1);
    const std::string rest = host_port.substr(close + 1);
    if (rest.empty()) {
      out.port = (out.protocol == "https") ? "443" : "80";
    } else if (rest[0] == ':') {
      out.port = rest.substr(1);
    } else {
      return false;
    }
    return !out.host.empty() && valid_port(out.port);
  }

  const auto colon = host_port.find(':');
  if (colon == std::string::npos) {
    out.host = host_port;
    out.port = (out.protocol == "https") ? "443" : "80";
  } else {
    out.host = host_port.substr(0, colon);
    out.port = host_port.substr(colon + 1);
  }
  return !out.host.empty() && valid_port(out.port);
}

// The value to put in the Host header for a parsed host. parse_url strips the
// brackets from an IPv6 literal (the resolver wants it bare), but RFC 7230
// requires them back in the header, so "::1" has to be sent as "[::1]".
inline std::string host_header_value(const std::string &host) {
  if (host.find(':') == std::string::npos) return host;
  return "[" + host + "]";
}

// Resolve a redirect target (a Location header value) against the URL that
// produced it. Handles absolute URLs, protocol-relative ("//host/..."),
// root-relative ("/path") and path-relative ("sub/page") locations.
inline std::string resolve_redirect(const std::string &base, const std::string &location) {
  if (location.empty()) return base;
  if (location.find("://") != std::string::npos) return location;  // already absolute

  parsed_url b;
  if (!parse_url(base, b)) return location;

  // Protocol-relative: keep the base scheme, take host/path from the location.
  if (location.size() >= 2 && location[0] == '/' && location[1] == '/') return b.protocol + ":" + location;

  const std::string origin = b.protocol + "://" + b.host + ":" + b.port;
  if (location[0] == '/') return origin + location;  // root-relative

  // Path-relative: resolve against the directory of the base path.
  std::string dir = b.path;
  const auto slash = dir.rfind('/');
  dir = (slash == std::string::npos) ? "/" : dir.substr(0, slash + 1);
  return origin + dir + location;
}

}  // namespace check_http_internal
}  // namespace check_net
