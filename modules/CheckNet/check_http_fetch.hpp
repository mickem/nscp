// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <bytes/base64.hpp>
#include <net/http/client.hpp>
#include <string>

#include "check_http_internal.hpp"
#include "check_net_error.hpp"

namespace check_net {
namespace check_http_fetch {

// The request-build/classification core shared by check_http and the
// status-page checks (check_apache_status & co). The result-string vocabulary
// ("ok", "http_<code>", "error: <message>", plus check-specific refinements
// like "invalid_url"/"parse_error"/"no_match") is a documented filter contract
// in all of them, so it must only exist in one place.

inline void add_basic_auth(http::request &rq, const std::string &username, const std::string &password) {
  if (!username.empty() || !password.empty()) rq.add_header("Authorization", "Basic " + bytes::base64_encode(username + ":" + password));
}

// Classify a status code as "ok" or "http_<code>". check_http treats
// redirects it does not follow as ok (ok_below = 400); the status-page checks
// require a 2xx (ok_below = 300).
inline std::string classify_status(const long long code, const long long ok_below) {
  return (code >= 200 && code < ok_below) ? "ok" : "http_" + std::to_string(code);
}

// Boost.Asio surfaces system errors using the OS code page (e.g. Windows
// ANSI like "Ingen sådan värd är känd" on Swedish locales) and tacks on a
// build-path source location. Convert to UTF-8 and strip the location.
inline std::string error_result(const std::exception &e) { return std::string("error: ") + format_exception_message(e); }

// Connection options shared by the four status-page checks.
struct fetch_options {
  std::string username;
  std::string password;
  std::string tls_version = "tlsv1.2+";
  std::string verify_mode = "peer";
  std::string ca_file;
  int timeout = 30;
};

struct fetch_result {
  // "ok" when the endpoint answered 2xx; otherwise "invalid_url",
  // "http_<code>" or "error: <message>". The vendor parsers later refine "ok"
  // into "parse_error" when the body is not the expected format.
  std::string result = "error";
  long long code = 0;
  std::string body;
  std::string host;
  long long port = 0;
};

// One-shot GET of a status page: no redirects, no body expectations, just
// {result, code, body}.
inline fetch_result fetch_status_page(const std::string &url, const fetch_options &opt) {
  fetch_result out;
  check_http_internal::parsed_url u;
  if (!check_http_internal::parse_url(url, u)) {
    out.result = "invalid_url";
    return out;
  }
  out.host = u.host;
  try {
    out.port = std::stoll(u.port);
    http::http_client_options options(u.protocol, opt.tls_version, opt.verify_mode, opt.ca_file);
    // add_fetch_options rejects non-positive timeouts; guard the cast anyway
    // so a future caller cannot turn -1 into a ~136-year unsigned timeout.
    options.timeout_seconds_ = static_cast<unsigned int>(opt.timeout > 0 ? opt.timeout : 1);
    http::simple_client client(options);

    http::request rq("GET", check_http_internal::host_header_value(u.host), u.path);
    rq.add_header("User-Agent", "NSClient++");
    add_basic_auth(rq, opt.username, opt.password);

    const http::response resp = client.fetch(u.host, u.port, rq);
    out.code = resp.status_code_;
    out.body = resp.payload_;
    out.result = classify_status(resp.status_code_, 300);
  } catch (const std::exception &e) {
    out.result = error_result(e);
  }
  return out;
}

}  // namespace check_http_fetch
}  // namespace check_net
