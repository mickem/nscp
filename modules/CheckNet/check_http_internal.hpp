/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Small pure helpers used by check_http; broken out so they can be unit
 * tested without dragging in Boost.Asio / OpenSSL.
 */

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
  const auto colon = host_port.find(':');
  if (colon == std::string::npos) {
    out.host = host_port;
    out.port = (out.protocol == "https") ? "443" : "80";
  } else {
    out.host = host_port.substr(0, colon);
    out.port = host_port.substr(colon + 1);
  }
  return !out.host.empty();
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
