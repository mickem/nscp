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

#include "icinga.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/json.hpp>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace json = boost::json;

namespace icinga {

int map_exit_status(const int nagios_status, const bool is_host) {
  if (is_host) {
    // Icinga 2 (lib/icinga/apiactions.cpp) only accepts 0 (UP) or 1 (DOWN) for hosts.
    return (nagios_status == 0) ? 0 : 1;
  }
  // Services accept 0..3; clamp anything outside that range to UNKNOWN (3).
  if (nagios_status < 0 || nagios_status > 3) return 3;
  return nagios_status;
}

std::vector<std::string> split_perfdata(const std::string &perfdata) {
  std::vector<std::string> out;
  std::string cur;
  bool in_quotes = false;
  for (const char c : perfdata) {
    if (c == '\'') {
      in_quotes = !in_quotes;
      cur += c;
      continue;
    }
    if (c == ' ' && !in_quotes) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
      continue;
    }
    cur += c;
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

namespace {
// Escape a host or service name so it can be embedded inside a double-quoted
// string literal in an Icinga 2 filter expression.  Icinga 2 uses C-style
// escapes inside string literals, so backslash and double-quote are the only
// characters we have to handle for safe injection.
std::string filter_quote(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (const char c : s) {
    if (c == '\\' || c == '"') out += '\\';
    out += c;
  }
  return out;
}
}  // namespace

std::string build_check_result_body(const int nagios_status, const std::string &plugin_output, const std::string &perfdata, const std::string &check_source,
                                    const std::string &host, const std::string &service) {
  const bool is_host = service.empty();
  json::object body;
  body["type"] = is_host ? "Host" : "Service";

  std::string filter = "host.name==\"" + filter_quote(host) + "\"";
  if (!is_host) filter += " && service.name==\"" + filter_quote(service) + "\"";
  body["filter"] = filter;

  body["exit_status"] = map_exit_status(nagios_status, is_host);
  body["plugin_output"] = plugin_output;

  json::array perf_array;
  for (const auto &entry : split_perfdata(perfdata)) {
    perf_array.emplace_back(entry);
  }
  if (!perf_array.empty()) {
    body["performance_data"] = std::move(perf_array);
  }

  if (!check_source.empty()) {
    body["check_source"] = check_source;
  }

  return json::serialize(json::value(std::move(body)));
}

static json::array split_templates(const std::string &templates, const std::string &fallback) {
  json::array arr;
  std::vector<std::string> parts;
  const std::string &source = templates.empty() ? fallback : templates;
  boost::split(parts, source, boost::is_any_of(","), boost::token_compress_on);
  for (auto &p : parts) {
    boost::trim(p);
    if (!p.empty()) arr.emplace_back(p);
  }
  if (arr.empty()) arr.emplace_back(fallback);
  return arr;
}

std::string build_host_create_body(const std::string &host, const std::string &templates) {
  json::object body;
  body["templates"] = split_templates(templates, "generic-host");

  json::object attrs;
  attrs["address"] = host;
  body["attrs"] = std::move(attrs);
  return json::serialize(json::value(std::move(body)));
}

std::string build_service_create_body(const std::string &templates, const std::string &check_command) {
  json::object body;
  body["templates"] = split_templates(templates, "generic-service");

  json::object attrs;
  attrs["check_command"] = check_command.empty() ? std::string("dummy") : check_command;
  body["attrs"] = std::move(attrs);
  return json::serialize(json::value(std::move(body)));
}

submit_result parse_check_result_response(const std::string &body) {
  submit_result r;
  r.ok = false;
  if (body.empty()) {
    r.message = "Empty response from Icinga 2";
    return r;
  }
  try {
    // Use stream_parser rather than json::parse so that any trailing bytes
    // after the first complete JSON value (e.g. chunked-transfer "0\r\n\r\n"
    // residue that the HTTP client failed to strip) do not turn a successful
    // submission into a parse failure.
    json::stream_parser p;
    boost::system::error_code ec;
    p.write_some(body, ec);
    if (ec) throw std::runtime_error(ec.message());
    if (!p.done()) {
      r.message = "Incomplete Icinga 2 response: " + body.substr(0, 256);
      return r;
    }
    json::value v = p.release();
    if (v.is_object()) {
      const json::object &obj = v.as_object();
      auto it = obj.find("results");
      if (it != obj.end() && it->value().is_array()) {
        const json::array &arr = it->value().as_array();
        if (!arr.empty() && arr[0].is_object()) {
          const json::object &first = arr[0].as_object();
          int code = 0;
          auto cit = first.find("code");
          if (cit != first.end()) {
            if (cit->value().is_int64()) {
              code = static_cast<int>(cit->value().as_int64());
            } else if (cit->value().is_double()) {
              code = static_cast<int>(cit->value().as_double());
            } else if (cit->value().is_uint64()) {
              code = static_cast<int>(cit->value().as_uint64());
            }
          }
          std::string status_text;
          auto sit = first.find("status");
          if (sit != first.end() && sit->value().is_string()) {
            status_text = std::string(sit->value().as_string().c_str());
          }
          r.ok = (code == 200);
          r.message = status_text.empty() ? ("Icinga code: " + std::to_string(code)) : status_text;
          return r;
        }
      }
      // Server returned a structured error envelope without `results`.
      auto eit = obj.find("status");
      if (eit != obj.end() && eit->value().is_string()) {
        r.message = std::string(eit->value().as_string().c_str());
        return r;
      }
    }
    r.message = "Unexpected Icinga 2 response: " + body.substr(0, 256);
  } catch (const std::exception &e) {
    r.message = std::string("Failed to parse Icinga 2 response: ") + e.what();
  }
  return r;
}

std::string url_encode(const std::string &value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;
  for (const unsigned char c : value) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << static_cast<char>(c);
    } else {
      escaped << '%' << std::setw(2) << std::uppercase << static_cast<int>(c) << std::nouppercase << std::setw(0);
    }
  }
  return escaped.str();
}

}  // namespace icinga
