// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/asio.hpp>
#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#endif
#include <chrono>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>

namespace syslog_client {

// Transport / framing names as they appear in configuration.
const char *const transport_udp = "udp";
const char *const transport_tcp = "tcp";
const char *const transport_tls = "tls";
const char *const framing_octet_counted = "octet-counted";
const char *const framing_non_transparent = "non-transparent";

// Frame one syslog message for a stream transport. RFC 6587 octet counting
// ("LEN SP MSG") is the default and the only framing RFC 5425 permits over
// TLS; non-transparent framing (a trailing LF) exists for older TCP
// receivers only. The message itself is LF-free by the time it gets here -
// submit() replaces every control byte - so the trailer cannot collide with
// message content.
inline std::string frame_message(const std::string &framing, const std::string &msg) {
  if (framing == framing_non_transparent) return msg + "\n";
  return str::xtos(msg.size()) + " " + msg;
}

struct connection_data : public socket_helpers::connection_info {
  std::string severity;
  std::string facility;
  std::string tag_syntax;
  std::string message_syntax;
  std::string ok_severity, warn_severity, crit_severity, unknown_severity;

  // "udp" (default), "tcp" or "tls".
  std::string transport;
  // "octet-counted" (default) or "non-transparent"; stream transports only.
  std::string framing;
  // Non-empty when the transport/framing configuration is invalid. The
  // submission must then fail instead of guessing - the wrong guess would be
  // sending in the clear on a target the operator configured for TLS.
  std::string config_error;

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

    // Transport selection, most explicit first: the `transport` option, an
    // address scheme (tls://host), the tree-wide `use ssl` boolean, then the
    // historical UDP default. `use ssl` never downgrades: combined with
    // transport = tcp it upgrades to TLS, combined with an explicit udp it
    // is a contradiction and fails the submission.
    transport = boost::algorithm::to_lower_copy(arguments.get_string_data("transport"));
    if (transport.empty() && arguments.has_protocol()) transport = boost::algorithm::to_lower_copy(arguments.get_protocol());
    const bool use_ssl = arguments.get_bool_data("ssl");
    if (transport.empty()) transport = use_ssl ? transport_tls : transport_udp;
    if (transport == transport_tcp && use_ssl) transport = transport_tls;
    if (transport != transport_udp && transport != transport_tcp && transport != transport_tls)
      config_error = "Invalid transport '" + transport + "': expected udp, tcp or tls";
    else if (transport == transport_udp && use_ssl)
      config_error = "'use ssl' needs a stream transport: set transport = tls";

    // RFC 5425 assigns syslog-over-TLS its own port (6514); cleartext UDP
    // and TCP share the traditional 514.
    port_ = arguments.address.get_port_string(transport == transport_tls ? "6514" : "514");

    // The typed fields, not the free-form data map: destination_container
    // routes the well-known "timeout" and "retry" keys into these fields, so
    // the old get_int_data("timeout") lookup never saw a configured value
    // and the default won no matter what the operator set. A settings
    // target's `retries` key stays in the data map under its own name.
    timeout = arguments.timeout;
    retry = arguments.get_int_data("retries", arguments.retry);

    framing = boost::algorithm::to_lower_copy(arguments.get_string_data("framing"));
    if (framing.empty()) framing = framing_octet_counted;
    if (framing != framing_octet_counted && framing != framing_non_transparent) {
      if (config_error.empty()) config_error = "Invalid framing '" + framing + "': expected octet-counted or non-transparent";
    } else if (transport == transport_tls && framing == framing_non_transparent && config_error.empty()) {
      // RFC 5425 mandates octet counting; a receiver that only speaks
      // non-transparent framing has no business on the TLS port.
      config_error = "RFC 5425 requires octet-counted framing over TLS";
    }

    // TLS options follow the tree-wide target keys. The server certificate
    // is verified against `ca` by default (verify mode = peer, plus the
    // hostname pinning in send_stream()); the only ways out are the explicit
    // `verify mode = none` or `insecure = true` opt-outs.
    ssl.enabled = (transport == transport_tls);
    ssl.certificate = arguments.get_string_data("certificate");
    ssl.certificate_key = arguments.get_string_data("certificate key");
    ssl.certificate_key_format = arguments.get_string_data("certificate format", "PEM");
    ssl.ca_path = arguments.get_string_data("ca");
    ssl.allowed_ciphers = arguments.get_string_data("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    ssl.verify_mode = arguments.get_string_data("verify mode", "peer");
    ssl.tls_version = arguments.get_string_data("tls version", "1.2+");
    if (arguments.get_bool_data("insecure")) ssl.verify_mode = "none";

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
    ss << ", transport: " << transport;
    if (transport != transport_udp) ss << ", framing: " << framing;
    if (ssl.enabled) ss << ", " << ssl.to_string();
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

namespace detail {
// Connect with a deadline: the shared socket helpers cover timed reads and
// writes but connect/handshake there are synchronous, and a stream target
// that drops SYNs would otherwise hang a submission for the OS connect
// timeout (minutes). Mirrors socket_helpers::io::write_with_timeout.
template <typename Socket, typename Endpoints>
void connect_with_timeout(boost::asio::io_context &io_service, Socket &socket, const Endpoints &endpoints, const std::chrono::milliseconds duration) {
  boost::optional<boost::system::error_code> timer_result;
  boost::asio::steady_timer timer(io_service);
  timer.expires_after(duration);
  timer.async_wait([&timer_result](const auto &e) { socket_helpers::io::set_result(&timer_result, e); });

  boost::optional<boost::system::error_code> connect_result;
  boost::asio::async_connect(socket, endpoints, [&connect_result](const auto &e, const auto &) { connect_result = e; });

  io_service.restart();
  while (io_service.run_one()) {
    if (connect_result) {
      try {
        timer.cancel();
      } catch (...) {
      }
      if (*connect_result) throw boost::system::system_error(*connect_result);
      return;
    } else if (timer_result) {
      socket.close();
      throw socket_helpers::socket_exception("Timeout connecting to syslog server");
    }
  }
  throw socket_helpers::socket_exception("Failed to connect to syslog server");
}

#ifdef USE_SSL
// The TLS handshake with the same deadline treatment: a peer that accepts
// the TCP connection but never answers the ClientHello must not stall the
// submission pipeline.
template <typename Stream>
void handshake_with_timeout(boost::asio::io_context &io_service, Stream &stream, const std::chrono::milliseconds duration) {
  boost::optional<boost::system::error_code> timer_result;
  boost::asio::steady_timer timer(io_service);
  timer.expires_after(duration);
  timer.async_wait([&timer_result](const auto &e) { socket_helpers::io::set_result(&timer_result, e); });

  boost::optional<boost::system::error_code> handshake_result;
  stream.async_handshake(boost::asio::ssl::stream_base::client, [&handshake_result](const auto &e) { handshake_result = e; });

  io_service.restart();
  while (io_service.run_one()) {
    if (handshake_result) {
      try {
        timer.cancel();
      } catch (...) {
      }
      if (*handshake_result) throw boost::system::system_error(*handshake_result);
      return;
    } else if (timer_result) {
      stream.lowest_layer().close();
      throw socket_helpers::socket_exception("Timeout during TLS handshake with syslog server");
    }
  }
  throw socket_helpers::socket_exception("TLS handshake with syslog server failed");
}
#endif
}  // namespace detail

struct syslog_client_handler : public client::handler_interface {
  // The agent's trusted CA bundle, resolved once from ${ca-path} at module
  // load (SyslogClient::loadModuleEx) - same fallback as the SMTP client. A
  // target read from settings carries its own `ca` default, but a
  // command-line submission arrives with `ca` unset, and the shared SSL
  // context loads no trust anchors on its own - verification would then
  // fail closed but uselessly, instead of trusting the agent's own bundle.
  std::string default_ca;

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
    if (con.ssl.enabled && con.ssl.ca_path.empty()) con.ssl.ca_path = default_ca;

    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

    if (!con.config_error.empty()) {
      NSC_LOG_ERROR("Refusing syslog submission: " + con.config_error);
      nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), con.config_error);
      return true;
    }

    // The RFC 3164 HOSTNAME field: without it the receiver promotes the next
    // token - the tag - to origin host, so a tag template that expands check
    // output would let a monitored process pick which host the record is
    // filed under. The sender carries the module's configured `hostname`
    // setting; "-" (the RFC 5424 nil value) marks an unconfigured sender
    // rather than shifting the fields.
    std::string hostname = sender.get_host();
    if (hostname.empty()) hostname = "-";

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

      std::string line = con.parse_priority(severity, con.facility) + date + " " + hostname + " " + tag + " " + message;
      // Neutralise every control byte, not just CR/LF/NUL: a newline would
      // split the check result into extra syslog records (log injection),
      // and the remaining C0 bytes are how ANSI escape sequences and other
      // terminal tricks ride a log file into an operator's terminal.
      // Replace with spaces so the message text stays readable; the
      // receiver sees one plain syslog line per check. This is also what
      // makes the stream framings in frame_message() unambiguous.
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
      if (con.transport == transport_udp) {
        send_udp(con, messages);
        nscapi::protobuf::functions::set_response_good(*payload, "Data presumably sent successfully");
        return;
      }

      // One connect per submission, all messages framed onto the same
      // stream, retried as a unit: syslog has no per-message
      // acknowledgement, so the retry granularity is the connection.
      std::string error;
      for (int attempt = 0; attempt <= con.retry; ++attempt) {
        if (attempt > 0) NSC_DEBUG_MSG_STD("Retrying syslog submission, attempt " + str::xtos(attempt) + " of " + str::xtos(con.retry));
        try {
          send_stream(con, messages);
          nscapi::protobuf::functions::set_response_good(
              *payload, "Sent " + str::xtos(messages.size()) + " syslog message(s) over " + con.transport + " to " + con.get_endpoint_string());
          return;
        } catch (const std::exception &e) {
          error = utf8::utf8_from_native(e.what());
          NSC_LOG_ERROR("Failed to send syslog message over " + con.transport + " to " + con.get_endpoint_string() + ": " + error);
        }
      }
      nscapi::protobuf::functions::set_response_bad(*payload, "Failed to send syslog message(s) over " + con.transport + ": " + error);
    } catch (const std::runtime_error &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Socket error: " + utf8::utf8_from_native(e.what()));
    } catch (const std::exception &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Error: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Unknown error -- REPORT THIS!");
    }
  }

 private:
  static void send_udp(const connection_data &con, const std::list<std::string> &messages) {
    boost::asio::io_context io_service;
    boost::asio::ip::udp::resolver resolver(io_service);
    // No address-family restriction: an IPv6-only syslog server resolves and
    // works; the resolver orders candidates and the first one wins.
    boost::asio::ip::udp::endpoint receiver_endpoint = resolver.resolve(con.address, con.get_port()).begin()->endpoint();

    boost::asio::ip::udp::socket socket(io_service);
    socket.open(receiver_endpoint.protocol());

    for (const std::string &msg : messages) {
      NSC_DEBUG_MSG_STD("Sending data: " + msg);
      socket.send_to(boost::asio::buffer(msg), receiver_endpoint);
    }
  }

  static void send_stream(const connection_data &con, const std::list<std::string> &messages) {
    boost::asio::io_context io_service;
    boost::asio::ip::tcp::resolver resolver(io_service);
    const auto endpoints = resolver.resolve(con.get_address(), con.get_port());
    const std::chrono::milliseconds deadline(std::chrono::seconds(con.timeout));

#ifdef USE_SSL
    if (con.ssl.enabled) {
      boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23_client);
      std::list<std::string> errors;
      con.ssl.configure_ssl_context(ctx, errors);
      if (!errors.empty()) {
        std::string emsg;
        for (const std::string &e : errors) emsg += (emsg.empty() ? "" : "; ") + e;
        throw socket_helpers::socket_exception("TLS setup failed: " + emsg);
      }
      boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream(io_service, ctx);

      detail::connect_with_timeout(io_service, stream.lowest_layer(), endpoints, deadline);

      // SNI: a receiver terminating TLS for several names needs the server
      // name to pick the right certificate; without it it may answer with
      // its default cert and fail the hostname check below. Mirrors the
      // Graphite and HTTP clients.
      if (!con.get_address().empty()) {
        SSL_set_tlsext_host_name(stream.native_handle(), con.get_address().c_str());
      }
      // When peer verification is enabled, pin the certificate to the host
      // we resolved so a CA-signed cert from another host cannot
      // impersonate the target (MITM guard) - mirrors the shared socket
      // client.
      if ((con.ssl.get_verify_mode() & boost::asio::ssl::context_base::verify_peer) != 0) {
        stream.set_verify_callback(boost::asio::ssl::host_name_verification(con.get_address()));
      }
      detail::handshake_with_timeout(io_service, stream, deadline);

      for (const std::string &msg : messages) {
        const std::string framed = frame_message(con.framing, msg);
        NSC_DEBUG_MSG_STD("Sending data: " + msg);
        if (!socket_helpers::io::write_with_timeout(io_service, stream, stream.lowest_layer(), boost::asio::buffer(framed), deadline))
          throw socket_helpers::socket_exception("Timeout sending syslog message");
      }
      boost::system::error_code ignored;
      stream.shutdown(ignored);
      return;
    }
#else
    if (con.ssl.enabled) {
      throw socket_helpers::socket_exception("TLS requested (transport = tls) but NSClient++ was built without OpenSSL support");
    }
#endif

    boost::asio::ip::tcp::socket socket(io_service);
    detail::connect_with_timeout(io_service, socket, endpoints, deadline);

    for (const std::string &msg : messages) {
      const std::string framed = frame_message(con.framing, msg);
      NSC_DEBUG_MSG_STD("Sending data: " + msg);
      if (!socket_helpers::io::write_with_timeout(io_service, socket, socket, boost::asio::buffer(framed), deadline))
        throw socket_helpers::socket_exception("Timeout sending syslog message");
    }
    boost::system::error_code ignored;
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
  }
};
}  // namespace syslog_client
