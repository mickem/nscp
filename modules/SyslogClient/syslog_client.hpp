// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <str/format.hpp>

namespace syslog_client {
struct connection_data : public socket_helpers::connection_info {
  std::string severity;
  std::string facility;
  std::string tag_syntax;
  std::string message_syntax;
  std::string ok_severity, warn_severity, crit_severity, unknown_severity;

  typedef std::map<std::string, int> syslog_map;
  syslog_map facilities;
  syslog_map severities;

  std::string parse_priority(std::string severity_arg, std::string facility_arg) {
    // A name that is not in the tables is an operator typo (or a missing
    // option), and the fallback must not escalate: <0> is kernel.emergency,
    // which many receivers broadcast or page on. Degrade to user.notice
    // (<13>) instead - visible in the log, alarming no one.
    syslog_map::const_iterator cit1 = facilities.find(facility_arg);
    if (cit1 == facilities.end()) {
      NSC_LOG_ERROR("Undefined facility: " + facility_arg);
      return "<13>";
    }
    syslog_map::const_iterator cit2 = severities.find(severity_arg);
    if (cit2 == severities.end()) {
      NSC_LOG_ERROR("Undefined severity: " + severity_arg);
      return "<13>";
    }
    std::stringstream ss;
    ss << '<' << (cit1->second * 8 + cit2->second) << '>';
    return ss.str();
  }

  connection_data(client::destination_container arguments, client::destination_container sender) {
    address = arguments.address.host;
    port_ = arguments.address.get_port_string("514");
    timeout = arguments.get_int_data("timeout", 30);
    retry = arguments.get_int_data("retry", 3);
    severity = arguments.data["severity"];
    facility = arguments.data["facility"];
    tag_syntax = arguments.data["tag template"];
    message_syntax = arguments.data["message template"];

    ok_severity = arguments.data["ok severity"];
    warn_severity = arguments.data["warning severity"];
    crit_severity = arguments.data["critical severity"];
    unknown_severity = arguments.data["unknown severity"];

    facilities["kernel"] = 0;
    facilities["user"] = 1;
    facilities["mail"] = 2;
    facilities["system"] = 3;
    facilities["security"] = 4;
    facilities["internal"] = 5;
    facilities["printer"] = 6;
    facilities["news"] = 7;
    facilities["UUCP"] = 8;
    facilities["clock"] = 9;
    facilities["authorization"] = 10;
    facilities["FTP"] = 11;
    facilities["NTP"] = 12;
    facilities["audit"] = 13;
    facilities["alert"] = 14;
    facilities["clock"] = 15;
    facilities["local0"] = 16;
    facilities["local1"] = 17;
    facilities["local2"] = 18;
    facilities["local3"] = 19;
    facilities["local4"] = 20;
    facilities["local5"] = 21;
    facilities["local6"] = 22;
    facilities["local7"] = 23;
    severities["emergency"] = 0;
    severities["alert"] = 1;
    severities["critical"] = 2;
    severities["error"] = 3;
    severities["warning"] = 4;
    severities["notice"] = 5;
    severities["informational"] = 6;
    severities["debug"] = 7;
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "host: " << get_endpoint_string();
    ss << ", severity: " << severity;
    ss << ", facility: " << facility;
    ss << ", tag_syntax: " << tag_syntax;
    ss << ", message_syntax: " << message_syntax;
    return ss.str();
  }
};

struct g_data {
  std::string path;
  std::string value;
};

struct syslog_client_handler : public client::handler_interface {
  // The module's configured `hostname`, already expanded. Held here rather
  // than pushed through client::configuration::set_sender() because that
  // stores the value as a url and get_sender() runs it through net::parse(),
  // which splits on the first colon - an IPv6 literal from ${address_ipv6}
  // would reach the wire as "2001". Nothing else in this module reads the
  // sender container, so there is nothing to keep in sync.
  std::string hostname;

  void set_hostname(std::string value) { hostname = std::move(value); }

  bool query(client::destination_container _sender, client::destination_container _target, const PB::Commands::QueryRequestMessage &_request_message,
             PB::Commands::QueryResponseMessage &_response_message) {
    return false;
  }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) {
    const PB::Common::Header &request_header = request_message.header();
    // (target, sender): the first argument is the target's settings - the
    // address, facility, severity and templates the operator configured.
    // Passing them the other way round read every one of them from the
    // sender container, which carries none of them.
    connection_data con(target, sender);

    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

    // The RFC 3164 HOSTNAME field: without it the receiver promotes the next
    // token - the tag - to origin host, so a tag template that expands check
    // output would let a monitored process pick which host the record is
    // filed under.
    //
    // A source host named on the submission itself (--source-host, or a host
    // carried in the request header) identifies the machine the result is
    // about, so it wins; it arrives via set_host() and is not url-parsed.
    // Otherwise this agent speaks for itself and the configured `hostname`
    // applies. "-" (the RFC 5424 nil value) holds the position when neither
    // is known, rather than shifting the remaining fields left.
    std::string host_field = sender.get_host();
    if (host_field.empty()) host_field = hostname;
    if (host_field.empty()) host_field = "-";

    std::list<std::string> messages;

    for (const ::PB::Commands::QueryResponseMessage_Response &p : request_message.payload()) {
      boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
      std::string date = str::format::format_date(now, "%b %e %H:%M:%S");
      std::string tag = con.tag_syntax;
      std::string message = con.message_syntax;
      std::string nagios_msg = nscapi::protobuf::functions::query_data_to_nagios_string(p, nscapi::protobuf::functions::no_truncation);
      str::utils::replace(message, "%message%", nagios_msg);
      str::utils::replace(tag, "%message%", nagios_msg);

      std::string severity = con.severity;
      if (p.result() == PB::Common::ResultCode::OK) severity = con.ok_severity;
      if (p.result() == PB::Common::ResultCode::WARNING) severity = con.warn_severity;
      if (p.result() == PB::Common::ResultCode::CRITICAL) severity = con.crit_severity;
      if (p.result() == PB::Common::ResultCode::UNKNOWN) severity = con.unknown_severity;
      // A submission that sets `severity` but not the per-state overrides
      // should use it, not trip the undefined-severity fallback.
      if (severity.empty()) severity = con.severity;

      std::string line = con.parse_priority(severity, con.facility) + date + " " + host_field + " " + tag + " " + message;
      // Neutralise every control byte, not just CR/LF/NUL: a newline would
      // split the check result into extra syslog records (log injection),
      // and the remaining C0 bytes are how ANSI escape sequences and other
      // terminal tricks ride a log file into an operator's terminal.
      // Replace with spaces so the message text stays readable; the
      // receiver sees one plain syslog line per check.
      for (char &c : line) {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) c = ' ';
      }
      messages.push_back(std::move(line));
    }
    send(response_message.add_payload(), con, messages);
    return true;
  }

  bool exec(client::destination_container _sender, client::destination_container _target, const PB::Commands::ExecuteRequestMessage &_request_message,
            PB::Commands::ExecuteResponseMessage &_response_message) {
    return false;
  }

  bool metrics(client::destination_container _sender, client::destination_container _target, const PB::Metrics::MetricsMessage &_request_message) {
    return false;
  }

  void send(PB::Commands::SubmitResponseMessage::Response *payload, connection_data con, const std::list<std::string> &messages) {
    try {
      NSC_DEBUG_MSG_STD("Connection details: " + con.to_string());

      boost::asio::io_context io_service;
      boost::asio::ip::udp::resolver resolver(io_service);
      boost::asio::ip::udp::endpoint receiver_endpoint = resolver.resolve(boost::asio::ip::udp::v4(), con.address, con.get_port()).begin()->endpoint();

      boost::asio::ip::udp::socket socket(io_service);
      socket.open(boost::asio::ip::udp::v4());

      for (const std::string &msg : messages) {
        NSC_DEBUG_MSG_STD("Sending data: " + msg);
        socket.send_to(boost::asio::buffer(msg), receiver_endpoint);
      }
      nscapi::protobuf::functions::set_response_good(*payload, "Data presumably sent successfully");
    } catch (const std::runtime_error &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Socket error: " + utf8::utf8_from_native(e.what()));
    } catch (const std::exception &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Error: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Unknown error -- REPORT THIS!");
    }
  }
};
}  // namespace syslog_client