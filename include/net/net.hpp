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

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>
#include <utf8.hpp>

namespace net {

struct string_traits {
  static std::string protocol_suffix() { return "://"; }
  static std::string port_prefix() { return ":"; }
};
struct url {
  std::string protocol;
  std::string host;
  std::string path;
  std::string query;
  unsigned int port;
  url() : port(0) {}

  std::string to_string() const {
    std::stringstream ss;
    ss << protocol << string_traits::protocol_suffix() << host;
    if (port != 0) ss << string_traits::port_prefix() << port;
    ss << path;
    return ss.str();
  }

  unsigned int get_port() const { return port; }
  unsigned int get_port(unsigned int default_port) const {
    if (port == 0) return default_port;
    return port;
  }
  std::string get_host(std::string default_host = "127.0.0.1") const {
    if (!host.empty()) return utf8::cvt<std::string>(host);
    return default_host;
  }
  std::string get_port_string(std::string default_port) const {
    if (port != 0) return boost::lexical_cast<std::string>(port);
    return default_port;
  }
  std::string get_port_string() const { return boost::lexical_cast<std::string>(port); }

  void import(const url &n) {
    if (protocol.empty() && !n.protocol.empty()) protocol = n.protocol;
    if (host.empty() && !n.host.empty()) host = n.host;
    if (port == 0 && n.port != 0) port = n.port;
    if (path.empty() && !n.path.empty()) path = n.path;
    if (query.empty() && !n.query.empty()) query = n.query;
  }
  void apply(const url &n) {
    if (!n.protocol.empty()) protocol = n.protocol;
    if (!n.host.empty()) host = n.host;
    if (n.port != 0) port = n.port;
    if (!n.path.empty()) path = n.path;
    if (!n.query.empty()) query = n.query;
  }
};

inline url parse(const std::string &url_s, unsigned int default_port = 0) {
  url ret;
  const std::string prot_end("://");
  auto prot_i = std::search(url_s.begin(), url_s.end(), prot_end.begin(), prot_end.end());
  if (prot_i != url_s.end()) {
    ret.protocol = boost::algorithm::to_lower_copy(url_s.substr(0, prot_i - url_s.begin()));
    std::advance(prot_i, prot_end.length());
  } else {
    ret.protocol = "";
    prot_i = url_s.begin();
  }
  std::string k("/:");
  auto path_i = std::find_first_of(prot_i, url_s.end(), k.begin(), k.end());
  ret.host = std::string(prot_i, path_i);
  if (ret.protocol != "ini" && ret.protocol != "registry") {
    if ((path_i != url_s.end()) && (*path_i == ':')) {
      auto port_b = path_i;
      ++port_b;
      const auto tmp = std::find(path_i, url_s.end(), '/');
      const auto chunk = std::string(port_b, tmp);
      if (!chunk.empty() && chunk.find_first_not_of("0123456789") == std::string::npos) {
        ret.port = boost::lexical_cast<unsigned int>(chunk);
        path_i = tmp;
      }
    } else {
      ret.port = default_port;
    }
  }
  auto query_i = std::find(path_i, url_s.end(), '?');
  ret.path.assign(path_i, query_i);
  if (query_i != url_s.end()) ++query_i;
  ret.query.assign(query_i, url_s.end());
  return ret;
}
}  // namespace net
