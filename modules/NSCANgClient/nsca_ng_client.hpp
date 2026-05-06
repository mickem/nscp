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

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <client/command_line_parser.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/nagios.hpp>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <sstream>
#include <str/utf8.hpp>
#include <string>

#include "nsca_ng.hpp"

namespace nsca_ng_client {

struct connection_data : public socket_helpers::connection_info {
  std::string password;
  std::string identity;
  std::string sender_hostname;
  std::string tls_ciphers;
  bool use_psk;

  connection_data(client::destination_container arguments, client::destination_container sender) : use_psk(true) {
    address = arguments.address.host;
    port_ = arguments.address.get_port_string("5668");
    ssl.enabled = true;
    ssl.allowed_ciphers = arguments.get_string_data("allowed ciphers");
    ssl.certificate = arguments.get_string_data("certificate");
    ssl.certificate_key = arguments.get_string_data("certificate key");
    ssl.ca_path = arguments.get_string_data("ca");
    ssl.verify_mode = arguments.get_string_data("verify mode");
    timeout = arguments.get_int_data("timeout", 30);
    retry = arguments.get_int_data("retries", 3);
    password = arguments.get_string_data("password");
    identity = arguments.get_string_data("identity");
    use_psk = arguments.get_bool_data("use psk", true);
    tls_ciphers = arguments.get_string_data("ciphers");

    sender_hostname = sender.address.host;
    if (sender.has_data("host")) sender_hostname = sender.get_string_data("host");
    if (identity.empty()) identity = sender_hostname;
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "host: " << get_endpoint_string();
    ss << ", identity: " << identity;
    ss << ", use_psk: " << (use_psk ? "true" : "false");
    ss << ", sender: " << sender_hostname;
    return ss.str();
  }
};

// Default PSK cipher list used when no explicit ciphers are configured.
// Only TLS 1.2 PSK ciphersuites are listed — PSK is not directly available
// via the legacy SSL_CTX_set_psk_client_callback API under TLS 1.3.
static const char kDefaultPskCiphers[] = "PSK-AES256-CBC-SHA256:PSK-AES128-CBC-SHA256:PSK-AES256-CBC-SHA:PSK-AES128-CBC-SHA";

// PSK credential storage — thread_local so that each OS thread can hold its
// own active connection state independently.  The synchronous send() helper
// is always called from a single thread per invocation, so there is no
// concurrent access to these variables within one thread.
struct psk_credentials {
  std::string identity;
  std::string psk;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local psk_credentials current_psk_credentials;

// Generate a base64-encoded session ID (6 random bytes, 8 base64 chars).
inline std::string generate_session_id() {
  static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned char buf[6];
  RAND_bytes(buf, sizeof(buf));
  // Simple 3-byte → 4-char base64 encoding (two groups of 3 bytes → 8 chars)
  std::string out;
  out.reserve(8);
  for (int i = 0; i < 2; ++i) {
    const unsigned char b0 = buf[i * 3];
    const unsigned char b1 = buf[i * 3 + 1];
    const unsigned char b2 = buf[i * 3 + 2];
    out += base64_chars[(b0 >> 2) & 0x3F];
    out += base64_chars[((b0 << 4) | (b1 >> 4)) & 0x3F];
    out += base64_chars[((b1 << 2) | (b2 >> 6)) & 0x3F];
    out += base64_chars[b2 & 0x3F];
  }
  return out;
}

// PSK client callback: fills identity and key from thread-local storage.
inline unsigned int psk_client_cb(SSL * /*ssl*/, const char * /*hint*/, char *identity, unsigned int max_identity_len, unsigned char *psk,
                                  unsigned int max_psk_len) {
  const auto &creds = current_psk_credentials;
  const std::size_t id_len = std::min(creds.identity.size(), static_cast<std::size_t>(max_identity_len - 1));
  std::memcpy(identity, creds.identity.c_str(), id_len);
  identity[id_len] = '\0';

  const std::size_t psk_len = std::min(creds.psk.size(), static_cast<std::size_t>(max_psk_len));
  std::memcpy(psk, creds.psk.c_str(), psk_len);
  return static_cast<unsigned int>(psk_len);
}

// -----------------------------------------------------------------------
// nsca_ng_connection — synchronous TLS connection that speaks the
// NSCA-NG text protocol (MOIN / PUSH / QUIT).
// -----------------------------------------------------------------------
struct nsca_ng_connection {
  boost::asio::io_service io_service_;
  boost::asio::ssl::context ctx_;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket_;
  boost::asio::ip::tcp::resolver resolver_;

  explicit nsca_ng_connection(bool use_psk, const std::string &ciphers)
      : ctx_(boost::asio::ssl::context::tls_client), ssl_socket_(io_service_, ctx_), resolver_(io_service_) {
    if (use_psk) {
      // Force TLS 1.2: PSK ciphersuites are not available in TLS 1.3 via
      // the legacy SSL_CTX_set_psk_client_callback API.
      SSL_CTX_set_max_proto_version(ctx_.native_handle(), TLS1_2_VERSION);
      const std::string cipher_list = ciphers.empty() ? kDefaultPskCiphers : ciphers;
      SSL_CTX_set_cipher_list(ctx_.native_handle(), cipher_list.c_str());
      SSL_CTX_set_psk_client_callback(ctx_.native_handle(), psk_client_cb);
    }
  }

  void connect(const std::string &host, const std::string &port, bool use_psk, const std::string &ca) {
    const boost::asio::ip::tcp::resolver::query query(host, port);
    const boost::asio::ip::tcp::resolver::iterator endpoint_it = resolver_.resolve(query);

    boost::system::error_code error = boost::asio::error::host_not_found;
    for (auto it = endpoint_it; error && it != boost::asio::ip::tcp::resolver::iterator(); ++it) {
      ssl_socket_.lowest_layer().close();
      ssl_socket_.lowest_layer().connect(*it, error);
    }
    if (error) throw socket_helpers::socket_exception("Failed to connect to " + host + ":" + port + ": " + error.message());

    if (use_psk) {
      // With PSK the key itself mutually authenticates both sides; there is no
      // X.509 certificate to verify, so verify_none is correct here.
      ssl_socket_.set_verify_mode(boost::asio::ssl::verify_none);
    } else if (!ca.empty()) {
      // Certificate-based TLS: verify the server certificate against the
      // configured CA bundle and match the hostname.
      ctx_.load_verify_file(ca);
      ssl_socket_.set_verify_mode(boost::asio::ssl::verify_peer);
      ssl_socket_.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));
    } else {
      // No CA configured — skip verification.  This is insecure but mirrors
      // the behaviour of many monitoring agent configurations where a private
      // CA is not available.  Users should supply a CA path for production.
      ssl_socket_.set_verify_mode(boost::asio::ssl::verify_none);
    }

    ssl_socket_.handshake(boost::asio::ssl::stream_base::client, error);
    if (error) throw socket_helpers::socket_exception("TLS handshake failed: " + error.message());
  }

  void write_line(const std::string &line) {
    const std::string data = line + "\n";
    boost::asio::write(ssl_socket_, boost::asio::buffer(data));
  }

  std::string read_line() {
    boost::asio::streambuf buf;
    boost::asio::read_until(ssl_socket_, buf, '\n');
    std::istream is(&buf);
    std::string line;
    std::getline(is, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
  }

  void close() {
    boost::system::error_code ec;
    ssl_socket_.lowest_layer().close(ec);
  }
};

// -----------------------------------------------------------------------
// Client handler — implements client::handler_interface for NSCANgClient.
// -----------------------------------------------------------------------
struct nsca_ng_client_handler final : public client::handler_interface {
  bool query(client::destination_container, client::destination_container, const PB::Commands::QueryRequestMessage &, PB::Commands::QueryResponseMessage &) override {
    return false;
  }

  bool exec(client::destination_container, client::destination_container, const PB::Commands::ExecuteRequestMessage &, PB::Commands::ExecuteResponseMessage &) override {
    return false;
  }

  bool metrics(client::destination_container, client::destination_container, const PB::Metrics::MetricsMessage &) override { return false; }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) override {
    const PB::Common::Header &request_header = request_message.header();
    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
    connection_data con(target, sender);

    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Target configuration: " + target.to_string()); }

    send(response_message.add_payload(), con, request_message);
    return true;
  }

  static void send(PB::Commands::SubmitResponseMessage::Response *payload, const connection_data &con, const PB::Commands::SubmitRequestMessage &request_message) {
    try {
      // Store PSK credentials in thread-local storage before connecting.
      current_psk_credentials.identity = con.identity;
      current_psk_credentials.psk = con.password;

      nsca_ng_connection conn(con.use_psk, con.tls_ciphers);
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Connecting to: " + con.to_string()); }
      conn.connect(con.get_address(), con.get_port(), con.use_psk, con.ssl.ca_path);

      // MOIN handshake
      const std::string session_id = generate_session_id();
      const std::string moin_req = nsca_ng::build_moin_request(session_id);
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("C: " + moin_req); }
      conn.write_line(moin_req);

      const std::string moin_resp = conn.read_line();
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("S: " + moin_resp); }
      const auto moin_result = nsca_ng::parse_server_response(moin_resp);
      if (!moin_result.ok()) {
        nscapi::protobuf::functions::set_response_bad(*payload, "NSCA-NG handshake failed: " + moin_resp);
        return;
      }

      // Send each check result
      for (const auto &p : request_message.payload()) {
        std::string alias = p.alias();
        if (alias.empty()) alias = p.command();
        const int code = nscapi::protobuf::functions::gbp_to_nagios_status(p.result());
        const std::string output = nscapi::protobuf::functions::query_data_to_nagios_string(p, 4096);
        const std::string service = (alias == "host_check") ? std::string() : alias;
        const long ts = static_cast<long>(std::time(nullptr));

        const std::string cmd = nsca_ng::build_check_result_command(con.sender_hostname, service, code, output, ts);
        const std::string data = cmd + "\n";
        const std::string push_req = nsca_ng::build_push_request(data.size());

        NSC_TRACE_ENABLED() { NSC_TRACE_MSG("C: " + push_req); }
        conn.write_line(push_req);

        const std::string push_resp = conn.read_line();
        NSC_TRACE_ENABLED() { NSC_TRACE_MSG("S: " + push_resp); }
        const auto push_result = nsca_ng::parse_server_response(push_resp);
        if (!push_result.ok()) {
          nscapi::protobuf::functions::set_response_bad(*payload, "NSCA-NG PUSH rejected: " + push_resp);
          conn.close();
          return;
        }

        NSC_TRACE_ENABLED() { NSC_TRACE_MSG("C: " + cmd); }
        boost::asio::write(conn.ssl_socket_, boost::asio::buffer(data));

        const std::string cmd_resp = conn.read_line();
        NSC_TRACE_ENABLED() { NSC_TRACE_MSG("S: " + cmd_resp); }
        const auto cmd_result = nsca_ng::parse_server_response(cmd_resp);
        if (!cmd_result.ok()) {
          nscapi::protobuf::functions::set_response_bad(*payload, "NSCA-NG submission rejected: " + cmd_resp);
          conn.close();
          return;
        }
      }

      // QUIT
      conn.write_line("QUIT");
      const std::string quit_resp = conn.read_line();
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("S: " + quit_resp); }
      conn.close();

      nscapi::protobuf::functions::set_response_good(*payload, "Submission successful");
    } catch (const socket_helpers::socket_exception &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "NSCA-NG network error: " + std::string(e.what()));
    } catch (const boost::system::system_error &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "NSCA-NG error: " + std::string(e.what()));
    } catch (const std::exception &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Error: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Unknown error -- REPORT THIS!");
    }
  }
};

}  // namespace nsca_ng_client
