// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Plumbing shared by the status-page checks (check_apache_status,
// check_nginx_status, check_phpfpm_status, check_tomcat_status in
// check_webserver.cpp, and check_jvm in check_jvm.cpp). Each check lives in a
// translation unit of its own as far as it can: every modern_filter
// instantiation adds thousands of COFF sections, and five of them in one file
// overflowed MSVC's limit (C1128).

#include <boost/program_options.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

#include "check_http_fetch.hpp"

namespace check_net {
namespace check_webserver_common {

namespace po = boost::program_options;
using check_http_fetch::fetch_options;
using check_http_fetch::fetch_result;

// Fields every status-page filter record shares. The per-check filter_obj
// structs derive from this; register_status_vars/fill_status keep the
// keywords and the fetch_result plumbing in one place.
struct status_obj {
  std::string url;
  std::string host;
  long long port = 0;
  long long code = 0;
  std::string result;
};

template <class THandler>
void register_status_vars(THandler &registry) {
  registry.add_string_var("url", [](auto obj) { return obj->url; }, "Full URL that was requested");
  registry.add_string_var("host", [](auto obj) { return obj->host; }, "Host part of the URL");
  registry.add_string_var("result", [](auto obj) { return obj->result; }, "Result of the check: ok, parse_error, http_<code> or error: <message>");
  registry.add_int_var("port", parsers::where::type_int, [](auto obj) { return obj->port; }, "TCP port that was used");
  registry.add_int_var("code", parsers::where::type_int, [](auto obj) { return obj->code; }, "HTTP status code of the response");
}

inline void fill_status(status_obj &obj, const std::string &url, const fetch_result &r) {
  obj.url = url;
  obj.host = r.host;
  obj.port = r.port;
  obj.code = r.code;
  obj.result = r.result;
}

// Add the connection options shared by all status-page checks to the option
// description and wire up the shared defaults.
inline void add_fetch_options(po::options_description &desc, std::string *url, const std::string &default_url, fetch_options &opt,
                              const std::string &default_ca) {
  opt.ca_file = default_ca;
  // clang-format off
  desc.add_options()
    ("url", po::value<std::string>(url)->default_value(default_url), "URL of the status endpoint (http://host[:port]/path or https://...).")
    ("timeout", po::value<int>(&opt.timeout)->default_value(30)->notifier([](const int value) {
        // Fetching casts this to unsigned; without the check timeout=-1 would
        // silently become a ~136-year timeout instead of an error.
        if (value <= 0) throw po::error("timeout must be a positive number of seconds");
      }), "Connection/read timeout in seconds.")
    ("username", po::value<std::string>(&opt.username), "Username for HTTP Basic authentication.")
    ("password", po::value<std::string>(&opt.password), "Password for HTTP Basic authentication.")
    ("tls-version", po::value<std::string>(&opt.tls_version)->default_value("tlsv1.2+"),
        "TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).")
    ("verify", po::value<std::string>(&opt.verify_mode)->default_value("peer"),
        "Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.")
    ("ca", po::value<std::string>(&opt.ca_file)->default_value(default_ca), "Path to a CA bundle used to verify the server certificate.")
    ;
  // clang-format on
}

}  // namespace check_webserver_common
}  // namespace check_net
