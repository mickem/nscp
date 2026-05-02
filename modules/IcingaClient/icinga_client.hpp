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

#include <bytes/base64.hpp>
#include <net/http/client.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <ostream>
#include <sstream>
#include <string>

#include "icinga.hpp"

namespace icinga_client {

struct connection_data : socket_helpers::connection_info {
  std::string username;
  std::string password;
  std::string protocol;
  std::string base_path;
  std::string tls_version;
  std::string verify_mode;
  std::string ca;

  std::string sender_hostname;
  std::string check_source;
  std::string host_template;
  std::string service_template;
  std::string check_command;
  bool ensure_objects;

  connection_data(client::destination_container arguments, client::destination_container sender) : ensure_objects(false) {
    address = arguments.address.host;
    protocol = arguments.address.protocol;
    base_path = arguments.address.path;
    if (protocol == "https") {
      port_ = arguments.address.get_port_string("5665");
    } else if (protocol == "http") {
      port_ = arguments.address.get_port_string("5665");
    } else {
      // Default to https for Icinga 2.
      protocol = "https";
      port_ = arguments.address.get_port_string("5665");
    }
    timeout = arguments.get_int_data("timeout", 30);
    username = arguments.get_string_data("username");
    password = arguments.get_string_data("password");
    retry = arguments.get_int_data("retry", 3);
    tls_version = arguments.get_string_data("tls version", "1.3");
    verify_mode = arguments.get_string_data("verify mode");
    ca = arguments.get_string_data("ca");

    ensure_objects = arguments.get_bool_data("ensure_objects", false);
    host_template = arguments.get_string_data("host_template");
    service_template = arguments.get_string_data("service_template");
    check_command = arguments.get_string_data("check_command");
    check_source = arguments.get_string_data("check_source");

    // destination_container::set_string_data("host", ...) routes to
    // address.host (see command_line_parser.hpp), so the sender hostname is
    // read from the address rather than from the data map. Fall back to the
    // local host name when no override has been supplied (e.g. submit_icinga
    // invoked from the CLI without --hostname).
    sender_hostname = sender.get_host();
    if (sender_hostname.empty()) sender_hostname = sender.get_string_data("host");
    if (sender_hostname.empty()) sender_hostname = boost::asio::ip::host_name();
    if (check_source.empty()) check_source = sender_hostname;
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "protocol: " << protocol;
    ss << ", host: " << get_endpoint_string();
    ss << ", port: " << port_;
    ss << ", timeout: " << timeout;
    ss << ", user: " << username;
    ss << ", sender: " << sender_hostname;
    ss << ", ensure_objects: " << (ensure_objects ? "true" : "false");
    ss << ", tls version: " << tls_version;
    ss << ", verify mode: " << verify_mode;
    return ss.str();
  }
};

inline std::string make_basic_auth(const std::string &user, const std::string &pwd) { return std::string("Basic ") + bytes::base64_encode(user + ":" + pwd); }

struct http_response {
  unsigned int status{};
  std::string body;
};

inline http_response do_http(const connection_data &con, const std::string &verb, const std::string &path, const std::string &json_body) {
  http_response result;
  result.status = 0;
  http::http_client_options options(con.protocol, con.tls_version, con.verify_mode, con.ca);
  http::simple_client c(options);

  http::request request(verb, con.get_address(), path);
  request.add_header("Authorization", make_basic_auth(con.username, con.password));
  request.add_header("Accept", "application/json");
  if (!json_body.empty()) {
    // add_post_payload(content_type, body) sets verb to POST; we re-assert the
    // verb afterwards because Icinga 2 uses both POST (actions) and PUT (object
    // create) with JSON bodies.
    request.add_post_payload("application/json", json_body);
    request.verb_ = verb;
  }

  http::response response = c.fetch(con.get_address(), con.get_port(), request);
  result.status = response.status_code_;
  result.body = response.payload_;
  return result;
}

struct icinga_client_handler : client::handler_interface {
  bool query(client::destination_container, client::destination_container, const PB::Commands::QueryRequestMessage &,
             PB::Commands::QueryResponseMessage &) override {
    return false;
  }

  bool exec(client::destination_container, client::destination_container, const PB::Commands::ExecuteRequestMessage &,
            PB::Commands::ExecuteResponseMessage &) override {
    return false;
  }

  bool metrics(client::destination_container, client::destination_container, const PB::Metrics::MetricsMessage &) override { return false; }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) override {
    const PB::Common::Header &request_header = request_message.header();
    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
    connection_data con(target, sender);

    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Sender configuration: " + sender.to_string()); }
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Target configuration: " + target.to_string()); }

    for (const ::PB::Commands::QueryResponseMessage_Response &p : request_message.payload()) {
      submit_one(response_message.add_payload(), con, p);
    }
    return true;
  }

  static void submit_one(PB::Commands::SubmitResponseMessage::Response *payload, const connection_data &con,
                         const PB::Commands::QueryResponseMessage::Response &p) {
    try {
      std::string alias = p.alias();
      if (alias.empty()) alias = p.command();
      const int nagios_result = nscapi::protobuf::functions::gbp_to_nagios_status(p.result());
      const std::string message = nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation);

      // Extract the perfdata from the rendered nagios string (after the | separator) so
      // we can emit it as a JSON array; Icinga 2 accepts both formats.
      std::string plugin_output = message;
      std::string perfdata;
      const auto pipe = message.find('|');
      if (pipe != std::string::npos) {
        plugin_output = message.substr(0, pipe);
        perfdata = message.substr(pipe + 1);
      }

      const bool is_host = (alias == "host_check");
      const std::string host = con.sender_hostname.empty() ? sender_default(con) : con.sender_hostname;

      if (con.ensure_objects) {
        if (!ensure_host(con, host)) {
          nscapi::protobuf::functions::set_response_bad(*payload, "Failed to ensure host object: " + host);
          return;
        }
        if (!is_host) {
          if (!ensure_service(con, host, alias)) {
            nscapi::protobuf::functions::set_response_bad(*payload, "Failed to ensure service object: " + host + "!" + alias);
            return;
          }
        }
      }

      // Use the filter form (type + filter in the body) on a fixed URL.  The
      // alternative ?host=/?service=host!service query form requires the exact
      // Icinga 2 __name of the object, which depends on how the object was
      // declared (apply rules in particular).  The filter form matches by the
      // user-visible host.name / service.name and just works.
      const std::string body = icinga::build_check_result_body(nagios_result, plugin_output, perfdata, con.check_source, host, is_host ? std::string() : alias);
      const std::string path = "/v1/actions/process-check-result";

      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("POST " + path + " " + body); }
      http_response res = do_http(con, "POST", path, body);
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("HTTP " + str::xtos(res.status) + ": " + res.body); }

      icinga::submit_result parsed = icinga::parse_check_result_response(res.body);
      if (parsed.ok && res.status >= 200 && res.status < 300) {
        nscapi::protobuf::functions::set_response_good(*payload, parsed.message);
      } else {
        std::string err = parsed.message;
        if (err.empty()) err = "HTTP " + str::xtos(res.status);
        nscapi::protobuf::functions::set_response_bad(*payload, err);
      }
    } catch (const socket_helpers::socket_exception &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, std::string("Network error: ") + e.what());
    } catch (const std::runtime_error &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, std::string("Error: ") + e.what());
    } catch (const std::exception &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, std::string("Error: ") + e.what());
    } catch (...) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Unknown error -- REPORT THIS!");
    }
  }

  static std::string sender_default(const connection_data &con) {
    if (!con.sender_hostname.empty()) return con.sender_hostname;
    return con.check_source;
  }

  // GET first; only PUT if 404.  Icinga 2 returns HTTP 500 when an object
  // already exists (not 409), so we cannot rely on the PUT response alone.
  static bool ensure_host(const connection_data &con, const std::string &host) {
    const std::string get_path = "/v1/objects/hosts/" + icinga::url_encode(host);
    try {
      const http_response existing = do_http(con, "GET", get_path, std::string());
      if (existing.status >= 200 && existing.status < 300) return true;
      if (existing.status != 404) {
        // 401/403 etc.  Surface these via a normal create attempt failure below.
      }
    } catch (...) {
      // fall through to create
    }
    const std::string body = icinga::build_host_create_body(host, con.host_template);
    const http_response created = do_http(con, "PUT", get_path, body);
    return created.status >= 200 && created.status < 300;
  }

  static bool ensure_service(const connection_data &con, const std::string &host, const std::string &service) {
    const std::string ident = host + "!" + service;
    const std::string get_path = "/v1/objects/services/" + icinga::url_encode(ident);
    try {
      const http_response existing = do_http(con, "GET", get_path, std::string());
      if (existing.status >= 200 && existing.status < 300) return true;
    } catch (...) {
      // fall through to create
    }
    const std::string body = icinga::build_service_create_body(con.service_template, con.check_command);
    const http_response created = do_http(con, "PUT", get_path, body);
    return created.status >= 200 && created.status < 300;
  }
};

}  // namespace icinga_client
