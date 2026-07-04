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
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "check_nsclient_web_online.h"

#include <NSCAPI.h>
#include <boost/algorithm/string.hpp>
#include <bytes/base64.hpp>
#include <cctype>
#include <memory>
#include <net/http/client.hpp>
#include <net/http/http_packet.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <sstream>

#include "check_net_error.hpp"

namespace check_net {

namespace {
// Minimal percent-encoding for query-parameter values (spaces and a few
// characters that would otherwise break the request line).
std::string url_encode(const std::string &s) {
  static const char *hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '=' || c == '&') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 0x0f]);
    }
  }
  return out;
}

int map_return_code(int nagios) {
  switch (nagios) {
    case 0:
      return NSCAPI::query_return_codes::returnOK;
    case 1:
      return NSCAPI::query_return_codes::returnWARN;
    case 2:
      return NSCAPI::query_return_codes::returnCRIT;
    default:
      return NSCAPI::query_return_codes::returnUNKNOWN;
  }
}
}  // namespace

void check_nsclient_web_online(const std::string &default_ca_file, const PB::Commands::QueryRequestMessage::Request &request,
                   PB::Commands::QueryResponseMessage::Response *response) {
  std::string url;
  std::string host;
  unsigned short port = 8443;
  std::string password;
  std::string user;
  std::string command;
  std::vector<std::string> arguments;
  std::string tls_version = "tlsv1.2+";
  std::string verify_mode = "none";
  std::string ca_file = default_ca_file;

  // NSClient++ passes options either as separate `--key value` tokens (the CLI
  // path) or as a single `key=value` token (the REST path). Parse both here,
  // rather than via boost::program_options whose grammar doesn't fit cleanly.
  // `argument` may repeat; every other option keeps its last value.
  const std::vector<std::string> raw_args(request.arguments().begin(), request.arguments().end());
  for (std::size_t i = 0; i < raw_args.size(); ++i) {
    std::string a = raw_args[i];
    while (!a.empty() && a[0] == '-') a.erase(0, 1);
    if (a.empty()) continue;
    const std::string::size_type eq = a.find('=');
    std::string key, val;
    if (eq != std::string::npos) {
      key = a.substr(0, eq);
      val = a.substr(eq + 1);
    } else {
      key = a;
      if (i + 1 < raw_args.size()) val = raw_args[++i];  // next token is the value
    }
    if (key == "url" || key == "u")
      url = val;
    else if (key == "host" || key == "H")
      host = val;
    else if (key == "port")
      try {
        port = static_cast<unsigned short>(std::stoi(val));
      } catch (...) {
      }
    else if (key == "password" || key == "p")
      password = val;
    else if (key == "user")
      user = val;
    else if (key == "command" || key == "c")
      command = val;
    else if (key == "argument" || key == "a" || key == "arguments")
      arguments.push_back(val);
    else if (key == "tls-version")
      tls_version = val;
    else if (key == "verify")
      verify_mode = val;
    else if (key == "ca")
      ca_file = val;
    // Unknown keys are ignored (forward-compatible).
  }

  // Resolve host/port/protocol from --url or --host.
  std::string protocol = "https";
  if (!url.empty()) {
    std::string rest = url;
    const std::string::size_type sep = rest.find("://");
    if (sep != std::string::npos) {
      protocol = rest.substr(0, sep);
      rest = rest.substr(sep + 3);
    }
    const std::string::size_type slash = rest.find('/');
    if (slash != std::string::npos) rest = rest.substr(0, slash);
    const std::string::size_type colon = rest.find(':');
    if (colon != std::string::npos) {
      host = rest.substr(0, colon);
      try {
        port = static_cast<unsigned short>(std::stoi(rest.substr(colon + 1)));
      } catch (...) {
      }
    } else {
      host = rest;
    }
  }
  if (host.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No host or url specified");

  // Build the request path: run a remote query, or hit /api/v1/info for a
  // reachability check.
  std::string path;
  if (command.empty()) {
    path = "/api/v1/info";
  } else {
    path = "/api/v1/queries/" + url_encode(command) + "/commands/execute";
    if (!arguments.empty()) {
      std::string query;
      for (const std::string &a : arguments) {
        if (!query.empty()) query += "&";
        query += url_encode(a);
      }
      path += "?" + query;
    }
  }

  const std::string base = protocol + "://" + host + ":" + std::to_string(port);
  try {
    http::http_client_options options(protocol, tls_version, verify_mode, ca_file);
    http::simple_client client(options);
    http::request rq("GET", host, path);
    rq.add_header("User-Agent", "NSClient++");
    if (!user.empty()) {
      rq.add_header("Authorization", "Basic " + bytes::base64_encode(user + ":" + password));
    } else if (!password.empty()) {
      rq.add_header("password", password);
    }

    client.connect(host, std::to_string(port));
    client.send_request(rq);

    boost::asio::streambuf response_buffer;
    const http::response resp = client.read_result(response_buffer);
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
    const std::string body = body_stream.str();

    if (resp.status_code_ == 401 || resp.status_code_ == 403) {
      return nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_nsclient_web_online", NSCAPI::query_return_codes::returnCRIT,
                                                                               "Authentication failed (HTTP " + std::to_string(resp.status_code_) + ") on " + base,
                                                                               "");
    }
    if (resp.status_code_ < 200 || resp.status_code_ >= 400) {
      return nscapi::protobuf::functions::append_simple_query_response_payload(
          response, "check_nsclient_web_online", NSCAPI::query_return_codes::returnCRIT, "REST API returned HTTP " + std::to_string(resp.status_code_) + " on " + base, "");
    }

    if (command.empty()) {
      return nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_nsclient_web_online", NSCAPI::query_return_codes::returnOK,
                                                                               "REST API reachable on " + base, "");
    }

    int remote_result = 3;
    std::string message;
    if (!parse_nsclient_web_online_result(body, remote_result, message)) {
      return nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_nsclient_web_online", NSCAPI::query_return_codes::returnUNKNOWN,
                                                                               "Could not parse response from " + base, "");
    }
    if (message.empty()) message = "(no message returned by " + command + ")";
    nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_nsclient_web_online", map_return_code(remote_result), message, "");
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_nsclient_web_online", NSCAPI::query_return_codes::returnCRIT,
                                                                      "Failed to reach " + base + ": " + check_net::format_exception_message(e), "");
  }
}

}  // namespace check_net
