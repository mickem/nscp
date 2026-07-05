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
#include <boost/program_options.hpp>
#include <bytes/base64.hpp>
#include <memory>
#include <net/http/client.hpp>
#include <net/http/http_packet.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>

#include "check_http_internal.hpp"
#include "check_http_json.hpp"
#include "check_net_error.hpp"

namespace po = boost::program_options;

namespace check_net {
namespace check_http_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("url", &filter_obj::get_url, "Full URL that was requested");
  registry_.add_string_var("host", &filter_obj::get_host, "Host part of the URL");
  registry_.add_string_var("path", &filter_obj::get_path, "Path part of the URL");
  registry_.add_string_var("protocol", &filter_obj::get_protocol, "Protocol used (http or https)");
  registry_.add_string_var("status", &filter_obj::get_status, "HTTP status message");
  registry_.add_string_var("body", &filter_obj::get_body, "Body of the response (use with substr/regex matching)");
  registry_.add_string_var("result", &filter_obj::get_result, "Textual result of the check (ok, error, ...)");
  registry_.add_int_var("port", parsers::where::type_int, &filter_obj::get_port, "TCP port that was used");
  // Perfdata labels: the record alias is the URL, so `time` (the primary
  // metric) keeps the bare alias and the others get a distinguishing suffix —
  // without one they would all collide on the same label.
  registry_.add_int_var("code", parsers::where::type_int, &filter_obj::get_code, "HTTP status code").add_int_perf("", "", "_code");
  registry_.add_int_var("time", parsers::where::type_int, &filter_obj::get_time, "Time taken by the request in milliseconds").add_int_perf("ms");
  registry_.add_int_var("size", parsers::where::type_int, &filter_obj::get_size, "Size of the response body in bytes").add_int_perf("B", "", "_size");
  registry_.add_int_var("ssl_expiry_days", parsers::where::type_int, &filter_obj::get_ssl_expiry_days,
                        "Days until the server's TLS certificate expires (-1 for plain http; negative if already expired)")
      .add_int_perf("", "", "_ssl_expiry_days");
}

}  // namespace check_http_filter

namespace {

using check_http_internal::parse_url;
using check_http_internal::parsed_url;
using check_http_internal::resolve_redirect;

// Options controlling a single HTTP check (kept in one struct so the growing
// argument set doesn't turn into a dozen positional parameters).
struct http_check_options {
  std::string method = "GET";
  std::string post_data;
  std::string content_type = "application/x-www-form-urlencoded";
  std::string username;
  std::string password;
  std::vector<std::string> headers;
  std::string expected_body;
  std::string user_agent = "NSClient++";
  std::string tls_version = "tlsv1.2+";
  std::string verify_mode = "none";
  std::string ca_file;
  std::string sni;
  bool follow_redirects = false;
  int max_redirs = 15;
  std::vector<std::pair<std::string, std::string>> json_paths;  // (alias, dotted path)
};

// Returns true for the 3xx status codes that carry a Location we should follow.
bool is_redirect(unsigned int code) { return code == 301 || code == 302 || code == 303 || code == 307 || code == 308; }

void run_http_check(const std::string &url_in, const http_check_options &opt, check_http_filter::filter_obj &out) {
  out.url = url_in;
  out.result = "error";

  const auto start = boost::chrono::steady_clock::now();
  std::string current = url_in;
  int redirects = 0;

  try {
    while (true) {
      parsed_url u;
      if (!parse_url(current, u)) {
        out.result = "invalid_url";
        return;
      }
      out.host = u.host;
      out.port = std::stoll(u.port);
      out.path = u.path;
      out.protocol = u.protocol;

      http::http_client_options options(u.protocol, opt.tls_version, opt.verify_mode, opt.ca_file);
      if (!opt.sni.empty()) options.sni_ = opt.sni;
      http::simple_client client(options);

      http::request rq(opt.method, u.host, u.path);
      if (!opt.user_agent.empty()) rq.add_header("User-Agent", opt.user_agent);
      if (!opt.username.empty() || !opt.password.empty())
        rq.add_header("Authorization", "Basic " + bytes::base64_encode(opt.username + ":" + opt.password));
      for (const auto &h : opt.headers) {
        const auto pos = h.find(':');
        if (pos == std::string::npos) continue;
        std::string k = h.substr(0, pos);
        std::string v = h.substr(pos + 1);
        boost::trim(k);
        boost::trim(v);
        if (!k.empty()) rq.add_header(k, v);
      }
      if (!opt.post_data.empty()) {
        rq.set_payload(opt.post_data);
        rq.add_header("Content-Length", std::to_string(opt.post_data.size()));
        rq.add_header("Content-Type", opt.content_type);
      }

      // fetch() connects, sends, reads the full (de-chunked) body and does NOT
      // throw on non-2xx — we want to inspect any status code / body ourselves.
      const http::response resp = client.fetch(u.host, u.port, rq);
      out.ssl_expiry_days = client.peer_certificate_expiry_days();

      // Follow redirects when asked to, up to the configured limit.
      if (opt.follow_redirects && redirects < opt.max_redirs && is_redirect(resp.status_code_)) {
        const auto loc = resp.headers_.find("location");
        if (loc != resp.headers_.end() && !loc->second.empty()) {
          current = resolve_redirect(current, loc->second);
          ++redirects;
          continue;
        }
      }

      out.status_code = resp.status_code_;
      out.status_message = resp.status_message_;
      out.body = resp.payload_;
      out.size = static_cast<long long>(out.body.size());

      if (!opt.json_paths.empty()) {
        check_http_json::extraction ex;
        if (check_http_json::extract(out.body, opt.json_paths, ex)) {
          out.json_numbers = std::move(ex.numbers);
          out.json_strings = std::move(ex.strings);
        }
      }

      if (!opt.expected_body.empty() && out.body.find(opt.expected_body) == std::string::npos) {
        out.result = "no_match";
      } else if (resp.status_code_ >= 200 && resp.status_code_ < 400) {
        out.result = "ok";
      } else {
        out.result = "http_" + std::to_string(resp.status_code_);
      }
      break;
    }
  } catch (const std::exception &e) {
    // Boost.Asio surfaces system errors using the OS code page (e.g. Windows
    // ANSI like "Ingen sådan värd är känd" on Swedish locales) and tacks on a
    // build-path source location. Convert to UTF-8 and strip the location.
    out.result = std::string("error: ") + check_net::format_exception_message(e);
  }

  const auto elapsed = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::steady_clock::now() - start).count();
  out.time = static_cast<long long>(elapsed);
}

}  // namespace

void check_http(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                PB::Commands::QueryResponseMessage::Response *response) {
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
  bool use_ssl = false;
  std::string onredirect = "ok";
  std::string ca_file;
  std::vector<std::string> json_paths_raw;

  http_check_options opt;

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
    ("ssl", po::value<bool>(&use_ssl)->implicit_value(true)->default_value(false), "Force https, alias for --protocol https (ssl=true).")
    ("timeout", po::value<int>(&timeout_ms)->default_value(30000), "Timeout in milliseconds.")
    ("method", po::value<std::string>(&opt.method)->default_value("GET"), "HTTP method to use (GET, HEAD, POST, PUT, DELETE, ...).")
    ("post-data", po::value<std::string>(&opt.post_data), "Request body to send; implies POST unless --method is given.")
    ("content-type", po::value<std::string>(&opt.content_type)->default_value("application/x-www-form-urlencoded"),
        "Content-Type header for the request body.")
    ("username", po::value<std::string>(&opt.username), "Username for HTTP Basic authentication.")
    ("password", po::value<std::string>(&opt.password), "Password for HTTP Basic authentication.")
    ("expected-body", po::value<std::string>(&opt.expected_body),
        "Substring that must appear in the body for the check to be ok.")
    ("user-agent", po::value<std::string>(&opt.user_agent)->default_value("NSClient++"), "User-Agent header value.")
    ("header", po::value<std::vector<std::string> >(&opt.headers),
        "Additional request header in 'Name: value' form (may be given multiple times).")
    ("onredirect", po::value<std::string>(&onredirect)->default_value("ok"),
        "How to handle 3xx redirects: 'follow' to follow the Location, 'ok' (default) to report the redirect as-is.")
    ("max-redirs", po::value<int>(&opt.max_redirs)->default_value(15), "Maximum number of redirects to follow (with --onredirect follow).")
    ("sni", po::value<std::string>(&opt.sni), "TLS Server Name Indication / verification hostname override (defaults to the URL host).")
    ("tls-version", po::value<std::string>(&opt.tls_version)->default_value("tlsv1.2+"),
        "TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).")
    ("verify", po::value<std::string>(&opt.verify_mode)->default_value("peer"),
        "Certificate verify mode: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.")
    ("ca", po::value<std::string>(&ca_file)->default_value(default_ca_file), "Path to a CA bundle to use when verifying the server certificate.")
    ("json-path", po::value<std::vector<std::string> >(&json_paths_raw),
        "Extract a value from the JSON response body as a filter keyword: 'alias:dotted.path' (repeatable). "
        "Numeric segments index arrays; single-quote a segment containing a dot. "
        "Example: --json-path qlen:data.queue.length \"crit=qlen > 100\".")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  for (const std::string &jp : json_paths_raw) {
    const auto pos = jp.find(':');
    if (pos == std::string::npos || pos == 0) return nscapi::protobuf::functions::set_response_bad(*response, "Invalid --json-path (expected alias:path): " + jp);
    opt.json_paths.emplace_back(jp.substr(0, pos), jp.substr(pos + 1));
  }

  opt.ca_file = ca_file;
  opt.follow_redirects = boost::algorithm::to_lower_copy(onredirect) == "follow";
  // A body without an explicit method means POST (matches curl / check_http_go).
  if (!opt.post_data.empty() && opt.method == "GET") opt.method = "POST";
  boost::algorithm::to_upper(opt.method);

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

  // Register each --json-path alias as a filter keyword on this check's own
  // handler (the context is per-filter, so this is safe). Numeric comparisons
  // use the float value, string comparisons the raw value; each alias also
  // becomes a perfdata metric. Must happen before build_filter parses the
  // warn/crit expressions that reference these aliases.
  for (const auto &ap : opt.json_paths) {
    const std::string alias = ap.first;
    auto var = std::make_shared<parsers::where::filter_variable<std::shared_ptr<filter_obj> > >(alias, parsers::where::type_float, "JSON value at " + ap.second);
    var->f_function = [alias](std::shared_ptr<filter_obj> o, parsers::where::evaluation_context) { return o->get_json_number(alias); };
    var->i_function = [alias](std::shared_ptr<filter_obj> o, parsers::where::evaluation_context) { return static_cast<long long>(o->get_json_number(alias)); };
    var->s_function = [alias](std::shared_ptr<filter_obj> o, parsers::where::evaluation_context) { return o->get_json_string(alias); };
    f.context->registry_.add(var, false);
    f.context->registry_.add_int_perf("", alias + "_", "");
  }

  if (!filter_helper.build_filter(f)) return;

  // The default warn/crit expressions reference time and code, which makes
  // them threshold-derived perfdata; size is not referenced by any default
  // expression so it must be registered explicitly to be graphed by default
  // (suppress with perf-config: size(ignored:true)).
  f.add_manual_perf("size");

  for (const auto &u : urls) {
    auto obj = std::make_shared<filter_obj>();
    run_http_check(u, opt, *obj);
    f.match(obj);
  }

  filter_helper.post_process(f);
}

}  // namespace check_net
