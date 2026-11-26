// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <bytes/base64.hpp>
#include <net/http/http_request.hpp>
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
  // When set the page is POSTed with this body and Content-Type instead of
  // fetched with a GET (Jolokia takes its bulk read as a JSON POST).
  std::string post_body;
  std::string post_content_type = "application/json";
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

// One-shot GET (or, with fetch_options::post_body, POST) of a status page: no
// redirects, no body expectations, just {result, code, body}.
//
// Defined out of line in check_http_fetch.cpp on purpose: the HTTP client
// drags in Boost.Asio, Beast and asio::ssl, which cost roughly 9,000 COFF
// sections in every translation unit that instantiates it. Each status-page
// check file is already close to MSVC's per-object section limit (C1128), so
// only this one file pays for the client.
fetch_result fetch_status_page(const std::string &url, const fetch_options &opt);

}  // namespace check_http_fetch
}  // namespace check_net
