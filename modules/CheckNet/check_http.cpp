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

#include "check_http.h"

#include <boost/algorithm/string.hpp>
#include <boost/chrono.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <net/http/client.hpp>
#include <net/http/http_packet.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>

#include "check_http_internal.hpp"
#include "check_net_error.hpp"

namespace po = boost::program_options;

namespace check_net {
namespace check_http_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("url", &filter_obj::get_url, "Full URL that was requested");
  registry_.add_string("host", &filter_obj::get_host, "Host part of the URL");
  registry_.add_string("path", &filter_obj::get_path, "Path part of the URL");
  registry_.add_string("protocol", &filter_obj::get_protocol, "Protocol used (http or https)");
  registry_.add_string("status", &filter_obj::get_status, "HTTP status message");
  registry_.add_string("body", &filter_obj::get_body, "Body of the response (use with substr/regex matching)");
  registry_.add_string("result", &filter_obj::get_result, "Textual result of the check (ok, error, ...)");
  registry_.add_int_x("port", parsers::where::type_int, &filter_obj::get_port, "TCP port that was used");
  registry_.add_int_x("code", parsers::where::type_int, &filter_obj::get_code, "HTTP status code");
  registry_.add_int_x("time", parsers::where::type_int, &filter_obj::get_time, "Time taken by the request in milliseconds");
  registry_.add_int_x("size", parsers::where::type_int, &filter_obj::get_size, "Size of the response body in bytes");
}

}  // namespace check_http_filter

namespace {

using check_http_internal::parse_url;
using check_http_internal::parsed_url;

void run_http_check(const std::string &url_in, int /*timeout_ms*/, const std::vector<std::string> &headers, const std::string &expected_body,
                    const std::string &user_agent, const std::string &tls_version, const std::string &verify_mode, const std::string &ca_file,
                    check_http_filter::filter_obj &out) {
  out.url = url_in;
  out.result = "error";

  parsed_url u;
  if (!parse_url(url_in, u)) {
    out.result = "invalid_url";
    return;
  }

  out.host = u.host;
  out.port = std::stoll(u.port);
  out.path = u.path;
  out.protocol = u.protocol;

  try {
    http::http_client_options options(u.protocol, tls_version, verify_mode, ca_file);
    http::simple_client client(options);
    http::packet rq("GET", u.host, u.path);
    if (!user_agent.empty()) rq.add_header("User-Agent", user_agent);
    for (const auto &h : headers) {
      const auto pos = h.find(':');
      if (pos == std::string::npos) continue;
      std::string k = h.substr(0, pos);
      std::string v = h.substr(pos + 1);
      boost::trim(k);
      boost::trim(v);
      if (!k.empty()) rq.add_header(k, v);
    }

    const auto start = boost::chrono::steady_clock::now();
    client.connect(u.host, u.port);
    client.send_request(rq);

    boost::asio::streambuf response_buffer;
    const http::response resp = client.read_result(response_buffer);

    // Drain remaining body.
    std::ostringstream body_stream;
    if (response_buffer.size() > 0) body_stream << &response_buffer;
    if (client.is_open()) {
      boost::system::error_code error;
      while (true) {
        const std::size_t n = client.read_some(response_buffer, error);
        if (n == 0) break;
        body_stream << &response_buffer;
      }
    }

    const auto elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::steady_clock::now() - start).count();
    out.time = static_cast<long long>(elapsed);
    out.status_code = resp.status_code_;
    out.status_message = resp.status_message_;
    out.body = body_stream.str();
    out.size = static_cast<long long>(out.body.size());

    if (!expected_body.empty() && out.body.find(expected_body) == std::string::npos) {
      out.result = "no_match";
    } else if (resp.status_code_ >= 200 && resp.status_code_ < 400) {
      out.result = "ok";
    } else {
      out.result = "http_" + std::to_string(resp.status_code_);
    }
  } catch (const std::exception &e) {
    // Boost.Asio surfaces system errors using the OS code page (e.g. Windows
    // ANSI like "Ingen sådan värd är känd" on Swedish locales) and tacks on a
    // build-path source location. Convert to UTF-8 and strip the location.
    out.result = std::string("error: ") + check_net::format_exception_message(e);
  }
}

}  // namespace

void check_http(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using check_http_filter::filter;
  using check_http_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  std::vector<std::string> urls;
  std::string host;
  std::string path = "/";
  std::string protocol = "http";
  unsigned short port = 0;
  int timeout_ms = 30000;
  std::string expected_body;
  std::string user_agent = "NSClient++";
  std::vector<std::string> headers;
  bool use_ssl = false;
  std::string tls_version = "tlsv1.2+";
  std::string verify_mode = "none";
  std::string ca_file;

  filter f;
  filter_helper.add_options("time > 5000", "code < 200 or code >= 400 or result != 'ok'", "", f.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${problem_list}", "${url} -> ${code} ${result} (${size}B in ${time}ms)", "${url}", "No URL checked",
                           "%(status): %(list)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("url", po::value<std::vector<std::string> >(&urls),
        "Full URL to check (http://host[:port]/path or https://...). May be given multiple times.")
    ("host", po::value<std::string>(&host), "Hostname (used when --url is not given).")
    ("port", po::value<unsigned short>(&port), "TCP port (defaults to 80 or 443).")
    ("path", po::value<std::string>(&path)->default_value("/"), "Path component of the URL.")
    ("protocol", po::value<std::string>(&protocol)->default_value("http"), "Protocol to use: http or https.")
    ("ssl", po::bool_switch(&use_ssl), "Force https (alias for --protocol https).")
    ("timeout", po::value<int>(&timeout_ms)->default_value(30000), "Timeout in milliseconds.")
    ("expected-body", po::value<std::string>(&expected_body),
        "Substring that must appear in the body for the check to be ok.")
    ("user-agent", po::value<std::string>(&user_agent)->default_value("NSClient++"), "User-Agent header value.")
    ("header", po::value<std::vector<std::string> >(&headers),
        "Additional request header in 'Name: value' form (may be given multiple times).")
    ("tls-version", po::value<std::string>(&tls_version)->default_value("tlsv1.2+"),
        "TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).")
    ("verify", po::value<std::string>(&verify_mode)->default_value("none"),
        "Certificate verify mode: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.")
    ("ca", po::value<std::string>(&ca_file), "Path to a CA bundle to use when verifying the server certificate.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (urls.empty()) {
    if (host.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No URL or host specified");
    if (use_ssl) protocol = "https";
    boost::algorithm::to_lower(protocol);
    if (protocol != "http" && protocol != "https") return nscapi::protobuf::functions::set_response_bad(*response, "Invalid protocol: " + protocol);
    if (port == 0) port = (protocol == "https") ? 443 : 80;
    std::string built = protocol + "://" + host + ":" + std::to_string(port);
    if (!path.empty() && path[0] != '/') built += "/";
    built += path;
    urls.push_back(built);
  }

  if (!filter_helper.build_filter(f)) return;

  for (const auto &u : urls) {
    auto obj = boost::make_shared<filter_obj>();
    run_http_check(u, timeout_ms, headers, expected_body, user_agent, tls_version, verify_mode, ca_file, *obj);
    f.match(obj);
  }

  filter_helper.post_process(f);
}

}  // namespace check_net
