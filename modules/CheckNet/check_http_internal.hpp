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

}  // namespace check_http_internal
}  // namespace check_net
