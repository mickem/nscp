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

#include <net/http/client.hpp>
#include <net/http/proxy_config.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>

#include <sstream>

#include "nrdp.hpp"

namespace nrdp_client {
struct connection_data : socket_helpers::connection_info {
  std::string token;
  std::string protocol;
  std::string path;
  std::string tls_version;
  std::string verify_mode;
  std::string ca;
  std::string proxy_url;
  std::string no_proxy_str;

  std::string sender_hostname;

  connection_data(client::destination_container arguments, client::destination_container sender) {
    address = arguments.address.host;
    protocol = arguments.address.protocol;
    path = arguments.address.path;
    if (path.empty()) path = "/nrdp/server/";
    if (protocol == "https")
      port_ = arguments.address.get_port_string("443");
    else {
      protocol = "http";
      port_ = arguments.address.get_port_string("80");
    }
    timeout = arguments.get_int_data("timeout", 30);
    token = arguments.get_string_data("token");
    retry = arguments.get_int_data("retry", 3);
    tls_version = arguments.get_string_data("tls version");
    if (tls_version.empty()) tls_version = "1.2+";
    verify_mode = arguments.get_string_data("verify mode");
    ca = arguments.get_string_data("ca");
    proxy_url = arguments.get_string_data("proxy");
    no_proxy_str = arguments.get_string_data("no proxy");

    if (sender.has_data("host")) sender_hostname = sender.get_string_data("host");
  }

  /// Build proxy_config from the URL and no-proxy string stored in this object.
  http::proxy_config build_proxy_config() const {
    http::proxy_config proxy = http::parse_proxy_url(proxy_url);
    if (!no_proxy_str.empty()) {
      std::istringstream ss(no_proxy_str);
      std::string tok;
      while (std::getline(ss, tok, ',')) {
        const auto start = tok.find_first_not_of(" \t");
        const auto end = tok.find_last_not_of(" \t");
        if (start != std::string::npos) proxy.no_proxy.push_back(tok.substr(start, end - start + 1));
      }
    }
    return proxy;
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "protocol: " << protocol;
    ss << ", host: " << get_endpoint_string();
    ss << ", port: " << port_;
    ss << ", path: " << path;
    ss << ", timeout: " << timeout;
    ss << ", token: " << token;
    ss << ", sender: " << sender_hostname;
    ss << ", tls version: " << tls_version;
    ss << ", verify mode: " << verify_mode;
    if (!proxy_url.empty()) ss << ", proxy: " << proxy_url;
    if (!no_proxy_str.empty()) ss << ", no proxy: " << no_proxy_str;
    return ss.str();
  }
};

struct nrdp_client_handler : client::handler_interface {
  bool query(client::destination_container _sender, client::destination_container _target, const PB::Commands::QueryRequestMessage &_request_message,
             PB::Commands::QueryResponseMessage &_response_message) override {
    return false;
  }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) override {
    const PB::Common::Header &request_header = request_message.header();
    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
    connection_data con(target, sender);

    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Target configuration: " + target.to_string()); }

    nrdp::data nrdp_data;

    for (const ::PB::Commands::QueryResponseMessage_Response &p : request_message.payload()) {
      std::string msg = nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation);
      std::string alias = p.alias();
      if (alias.empty()) alias = p.command();
      int result = nscapi::protobuf::functions::gbp_to_nagios_status(p.result());
      if (alias == "host_check")
        nrdp_data.add_host(sender.get_host(), result, msg);
      else
        nrdp_data.add_service(sender.get_host(), alias, result, msg);
    }
    send(response_message.add_payload(), con, nrdp_data);
    return true;
  }

  bool exec(client::destination_container _sender, client::destination_container _target, const PB::Commands::ExecuteRequestMessage &_request_message,
            PB::Commands::ExecuteResponseMessage &_response_message) override {
    return false;
  }

  bool metrics(client::destination_container _sender, client::destination_container _target, const PB::Metrics::MetricsMessage &_request_message) override {
    return false;
  }

  static void send(PB::Commands::SubmitResponseMessage::Response *payload, const connection_data &con, const nrdp::data &nrdp_data) {
    try {
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Connecting tuo: " + con.to_string()); }
      http::http_client_options options(con.protocol, con.tls_version, con.verify_mode, con.ca, con.build_proxy_config());
      http::simple_client c(options);
      http::request request("POST", con.get_address(), con.path);
      http::request::post_map_type post;
      post["token"] = con.token;
      post["XMLDATA"] = nrdp_data.render_request();
      post["cmd"] = "submitcheck";
      request.add_post_payload(post);
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Sending: " + nrdp_data.render_request()); }
      std::ostringstream os;
      http::response response = c.execute(os, con.get_address(), con.get_port(), request);
      response.payload_ = os.str();
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Received: " + response.payload_); }
      boost::tuple<int, std::string> ret = nrdp::data::parse_response(response.payload_);
      if (ret.get<0>() != 0) {
        nscapi::protobuf::functions::set_response_bad(*payload, ret.get<1>());
      } else {
        nscapi::protobuf::functions::set_response_good(*payload, ret.get<1>());
      }
    } catch (const std::runtime_error &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Socket error: " + utf8::utf8_from_native(e.what()));
    } catch (const std::exception &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Error: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Unknown error -- REPORT THIS!");
    }
  }
};
}  // namespace nrdp_client