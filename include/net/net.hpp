// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>
#include <str/utf8.hpp>

namespace net {

struct string_traits {
  static std::string protocol_suffix() { return "://"; }
  static std::string port_prefix() { return ":"; }
};

// Percent-encode whatever is not legal in a url query, so the result can be put
// on an HTTP request line verbatim. RFC 3986 allows query = *( pchar / "/" /
// "?" ), i.e. unreserved / sub-delims / ":" / "@" / "/" / "?" / pct-encoded.
// Everything else either makes the request line unparseable (a space turns
// "GET /a?b=c d HTTP/1.0" into a malformed three-token line) or, for a stray CR
// or LF, splits one request into two. The query only reaches the wire since
// issue #460, so this guards a door that was previously closed by accident.
//
// An operator may well have written the query already encoded, so an existing
// "%XX" pair is passed through untouched rather than turned into "%25XX". A '%'
// that does not introduce a valid pair is not an escape and is encoded.
inline std::string encode_query(const std::string &query) {
  static const std::string sub_delims = "-._~!$&'()*+,;=:@/?";
  static const char hex[] = "0123456789ABCDEF";
  const auto is_hex = [](const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); };

  std::string out;
  out.reserve(query.size());
  for (std::string::size_type i = 0; i < query.size(); ++i) {
    const char c = query[i];
    if (c == '%' && i + 2 < query.size() && is_hex(query[i + 1]) && is_hex(query[i + 2])) {
      out.append(query, i, 3);
      i += 2;
      continue;
    }
    const auto uc = static_cast<unsigned char>(c);
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9') || sub_delims.find(c) != std::string::npos) {
      out.push_back(c);
      continue;
    }
    out.push_back('%');
    out.push_back(hex[(uc >> 4) & 0xf]);
    out.push_back(hex[uc & 0xf]);
  }
  return out;
}

struct url {
  std::string protocol;
  std::string host;
  std::string path;
  std::string query;
  unsigned int port;
  url() : port(0) {}

  // The full url, query string included. Prefer to_log_safe_string() for
  // anything that ends up in a log or an error message.
  std::string to_string() const { return get_baseurl() + get_request_path(); }

  // The url without the query string. A settings url is free to carry
  // credentials in its parameters (".../cfg.php?token=..."), and the settings
  // layer logs the url it is fetching on every boot - at warning level when TLS
  // verification is off or the CA bundle is missing. Identifying the source
  // does not need the parameters, so they are left out rather than written to
  // disk in clear text.
  std::string to_log_safe_string() const { return get_baseurl() + get_path(); }

  // Scheme and authority: "http://host:8080".
  std::string get_baseurl() const {
    std::stringstream ss;
    ss << protocol << string_traits::protocol_suffix() << host;
    if (port != 0) ss << string_traits::port_prefix() << port;
    return ss.str();
  }

  // The document path, without the query string.
  std::string get_path() const { return path; }

  // The resource as it has to appear on the HTTP request line: everything
  // after the authority, query string included. `path` on its own stops at
  // the '?', so a caller that hands it straight to a downloader silently
  // drops every parameter the user wrote (issue #460).
  std::string get_request_path() const {
    if (query.empty()) return path;
    return path + "?" + encode_query(query);
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
