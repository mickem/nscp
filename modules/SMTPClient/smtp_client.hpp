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

#include <net/socket/socket_helpers.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include "smtp.hpp"

namespace smtp_client {
struct connection_data : socket_helpers::connection_info {
  typedef connection_info parent;

  // Authentication / transport
  std::string username;
  std::string password;
  std::string security;  // "none" | "starttls" | "tls"
  bool insecure_skip_verify = false;
  std::string canonical_name;  // EHLO hostname

  // Message construction
  std::string sender;  // From: header / envelope sender
  std::string sender_hostname;
  std::string recipient_str;
  std::string subject_template;
  std::string template_string;

  connection_data(client::destination_container arguments, client::destination_container sender_container) {
    address = arguments.address.host;
    port_ = arguments.address.get_port_string("587");
    timeout = arguments.get_int_data("timeout", 30);
    retry = arguments.get_int_data("retry", 3);

    username = arguments.get_string_data("username");
    password = arguments.get_string_data("password");
    security = arguments.get_string_data("security");
    if (security.empty()) security = "starttls";
    insecure_skip_verify = arguments.get_bool_data("insecure-skip-verify");

    sender = arguments.get_string_data("sender");
    recipient_str = arguments.get_string_data("recipient");
    subject_template = arguments.get_string_data("subject");
    template_string = arguments.get_string_data("template");

    if (sender_container.has_data("host")) {
      sender_hostname = sender_container.get_string_data("host");
    }
    canonical_name = arguments.get_string_data("ehlo-hostname");
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "host: " << parent::to_string();
    ss << ", recipient: " << recipient_str;
    ss << ", sender: " << sender;
    ss << ", username: " << (username.empty() ? "<unset>" : username);
    ss << ", password: " << (password.empty() ? "<unset>" : "<set>");
    ss << ", security: " << security;
    ss << ", subject: " << subject_template;
    ss << ", template-len: " << template_string.size();
    return ss.str();
  }
};

struct smtp_client_handler : client::handler_interface {
  bool query(client::destination_container, client::destination_container, const PB::Commands::QueryRequestMessage&,
             PB::Commands::QueryResponseMessage&) override {
    return false;
  }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage& request_message,
              PB::Commands::SubmitResponseMessage& response_message) override {
    const PB::Common::Header& request_header = request_message.header();
    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

    connection_data con(target, sender);
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("SMTP target: " + con.to_string()); }

    smtp::connection_config cfg;
    cfg.server = con.address;
    cfg.port = con.port_;
    cfg.username = con.username;
    cfg.password = con.password;
    cfg.security = con.security;
    cfg.insecure_skip_verify = con.insecure_skip_verify;
    cfg.canonical_name = con.canonical_name.empty() ? con.sender_hostname : con.canonical_name;
    cfg.timeout_seconds = static_cast<int>(con.timeout);

    for (const ::PB::Commands::QueryResponseMessage_Response& p : request_message.payload()) {
      const std::string nagios_msg = nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation);
      const std::string source_alias = p.alias().empty() ? p.command() : p.alias();

      smtp::message msg;
      msg.from = con.sender;
      msg.to = con.recipient_str;
      msg.subject = con.subject_template;
      str::utils::replace(msg.subject, "%message%", nagios_msg);
      str::utils::replace(msg.subject, "%source%", source_alias);
      msg.body = con.template_string;
      str::utils::replace(msg.body, "%message%", nagios_msg);
      str::utils::replace(msg.body, "%source%", source_alias);

      try {
        smtp::send(cfg, msg);
        nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), source_alias, true, "Email sent successfully");
      } catch (const smtp::smtp_exception& e) {
        NSC_LOG_ERROR(std::string("SMTP send failed: ") + e.what());
        nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), source_alias, false,
                                                                           std::string("SMTP send failed: ") + e.what());
      }
    }
    return true;
  }

  bool exec(client::destination_container, client::destination_container, const PB::Commands::ExecuteRequestMessage&,
            PB::Commands::ExecuteResponseMessage&) override {
    return false;
  }

  bool metrics(client::destination_container, client::destination_container, const PB::Metrics::MetricsMessage&) override { return false; }
};
}  // namespace smtp_client
