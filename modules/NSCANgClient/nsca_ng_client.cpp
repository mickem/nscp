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

#include "nsca_ng_client.hpp"

#include <openssl/rand.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <chrono>
#include <cstring>
#include <ctime>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/nagios.hpp>
#include <stdexcept>
#include <str/utf8.hpp>
#include <thread>

#include "nsca_ng.hpp"

namespace nsca_ng_client {

// AEAD ciphersuites first (GCM, ChaCha20-Poly1305) — these aren't vulnerable
// to the padding-oracle / Lucky13-class attacks that have plagued CBC. CBC
// suites stay at the end as fallback for old servers that don't support GCM
// PSK. OpenSSL silently drops any cipher it can't link (e.g. ChaCha20 in
// builds without it), so listing them is safe.
//
// `@SECLEVEL=0` MUST appear before any cipher entries — it is a control
// directive that filters the entries that follow it, so a trailing
// position is silently ignored. Without it, OpenSSL 3's default seclevel
// strips the PSK suites at load time and SSL_CTX_set_cipher_list returns
// 0; OpenSSL then keeps its default cert-based cipher list and the next
// PSK handshake aborts with "no suitable signature algorithm" on the
// server.
const char kDefaultPskCiphers[] =
    "@SECLEVEL=0:"
    "PSK-AES256-GCM-SHA384:PSK-AES128-GCM-SHA256:PSK-CHACHA20-POLY1305:"
    "PSK-AES256-CBC-SHA256:PSK-AES128-CBC-SHA256:PSK-AES256-CBC-SHA:PSK-AES128-CBC-SHA";

// =====================================================================
// connection_data
// =====================================================================

connection_data::connection_data(client::destination_container arguments, client::destination_container sender) {
  address = arguments.address.host;
  port_ = arguments.address.get_port_string("5668");
  ssl.enabled = true;
  ssl.allowed_ciphers = arguments.get_string_data("allowed ciphers");
  ssl.certificate = arguments.get_string_data("certificate");
  ssl.certificate_key = arguments.get_string_data("certificate key");
  ssl.ca_path = arguments.get_string_data("ca");
  ssl.verify_mode = arguments.get_string_data("verify mode");
  // configure_ssl_context() throws on an empty tls_version; default to a sane
  // floor so cert-mode targets don't have to spell it out.
  ssl.tls_version = arguments.get_string_data("tls version");
  if (ssl.tls_version.empty()) ssl.tls_version = "tlsv1.2+";
  timeout = arguments.get_int_data("timeout", 30);
  retry = arguments.get_int_data("retries", kDefaultMaxAttempts - 1);
  password = arguments.get_string_data("password");
  identity = arguments.get_string_data("identity");
  use_psk = arguments.get_bool_data("use psk", true);
  insecure = arguments.get_bool_data("insecure", false);
  max_output_length = static_cast<std::size_t>(arguments.get_int_data("max output length", static_cast<int>(kDefaultMaxOutputBytes)));
  host_check_default = arguments.get_bool_data("host check", false);

  sender_hostname = sender.address.host;
  if (sender.has_data("host")) sender_hostname = sender.get_string_data("host");
  if (identity.empty()) identity = sender_hostname;
}

std::string connection_data::to_string() const {
  std::stringstream ss;
  ss << "host: " << get_endpoint_string();
  ss << ", identity: " << identity;
  ss << ", use_psk: " << (use_psk ? "true" : "false");
  ss << ", insecure: " << (insecure ? "true" : "false");
  ss << ", sender: " << sender_hostname;
  ss << ", timeout: " << timeout << "s";
  ss << ", retries: " << retry;
  ss << ", host_check_default: " << (host_check_default ? "true" : "false");
  return ss.str();
}

// =====================================================================
// PSK callback — per-connection via SSL_set_ex_data (L1 fix)
// =====================================================================

int get_psk_ex_data_index() {
  // Allocated once per process. The argument list is the documented OpenSSL
  // contract — argl/argp/new_func/dup_func/free_func — all unused here.
  static const int idx = SSL_get_ex_new_index(0L, nullptr, nullptr, nullptr, nullptr);
  return idx;
}

unsigned int psk_client_cb(SSL *ssl, const char * /*hint*/, char *identity, unsigned int max_identity_len, unsigned char *psk, unsigned int max_psk_len) {
  if (max_identity_len == 0 || max_psk_len == 0) return 0;

  const auto *creds = static_cast<const psk_credentials *>(SSL_get_ex_data(ssl, get_psk_ex_data_index()));
  if (creds == nullptr) return 0;

  const std::size_t id_len = std::min(creds->identity.size(), static_cast<std::size_t>(max_identity_len) - 1);
  std::memcpy(identity, creds->identity.c_str(), id_len);
  identity[id_len] = '\0';

  const std::size_t psk_len = std::min(creds->psk.size(), static_cast<std::size_t>(max_psk_len));
  std::memcpy(psk, creds->psk.c_str(), psk_len);
  return static_cast<unsigned int>(psk_len);
}

// =====================================================================
// session ID
// =====================================================================

std::string generate_session_id() {
  static constexpr char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned char buf[6];
  if (RAND_bytes(buf, sizeof(buf)) != 1) {
    throw std::runtime_error("OpenSSL RAND_bytes failed: cannot generate NSCA-NG session ID");
  }
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

// =====================================================================
// nsca_ng_connection — TLS connection with deadline-driven I/O.
// =====================================================================
//
// All blocking operations (connect, handshake, read, write) are issued as
// async ops with a deadline_timer. If the deadline fires first the socket is
// cancelled and we raise socket_exception("timed out") (B2 fix).
namespace {

class nsca_ng_connection {
 public:
  boost::asio::io_service io_service_;
  boost::asio::ssl::context ctx_;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket_;
  boost::asio::ip::tcp::resolver resolver_;
  boost::asio::deadline_timer timer_;
  // Cap the inbound buffer so a hostile or buggy server can't grow `in_buf_`
  // without limit by streaming data without a newline. NSCA-NG response lines
  // are short (single keyword + brief reason); 8 KiB is well over the maximum
  // legitimate frame.
  static constexpr std::size_t kMaxResponseLineBytes = 8 * 1024;
  boost::asio::streambuf in_buf_{kMaxResponseLineBytes};
  int timeout_seconds_;
  psk_credentials psk_creds_;
  bool tls_active_ = false;

  nsca_ng_connection(bool use_psk, const std::string &psk_cipher_list, const int timeout_seconds)
      : ctx_(boost::asio::ssl::context::tls_client),
        ssl_socket_(io_service_, ctx_),
        resolver_(io_service_),
        timer_(io_service_),
        timeout_seconds_(timeout_seconds) {
    if (use_psk) {
      // IMPORTANT: configure on the SSL object (`ssl_socket_.native_handle()`),
      // NOT the SSL_CTX. The boost::asio::ssl::stream constructor above already
      // called SSL_new(ctx) and copied the cipher list / seclevel / etc. out of
      // the CTX. Subsequent SSL_CTX_set_* calls would land on the CTX without
      // propagating to the live SSL — the actual ClientHello would carry
      // OpenSSL's default cert-based cipher list, and the server would alert
      // with "no suitable signature algorithm" because no PSK cipher matched.
      SSL *const ssl = ssl_socket_.native_handle();

      // PSK ciphersuites aren't reachable in TLS 1.3 via the legacy
      // SSL_set_psk_client_callback API; force TLS 1.2 only on both
      // ends of the negotiation.
      SSL_set_min_proto_version(ssl, TLS1_2_VERSION);
      SSL_set_max_proto_version(ssl, TLS1_2_VERSION);

      // Drop seclevel to 0 so OpenSSL 3 keeps the PSK suites in the cipher
      // list. PSK has no forward secrecy and several PSK-CBC suites use
      // SHA1 — OpenSSL 3 strips those at the default seclevel of 1, which
      // would leave the agent with an empty cipher list at handshake.
      SSL_set_security_level(ssl, 0);

      const std::string cipher_list = psk_cipher_list.empty() ? kDefaultPskCiphers : psk_cipher_list;
      // Throw on cipher-list parse failure rather than letting OpenSSL fall
      // back to its cert-based default. With a silent fallback the resulting
      // PSK handshakes alert with confusing messages like "no suitable
      // signature algorithm" — this gives the operator the actual cause.
      if (SSL_set_cipher_list(ssl, cipher_list.c_str()) != 1) {
        throw socket_helpers::socket_exception("Failed to apply PSK cipher list: " + cipher_list);
      }
      SSL_set_psk_client_callback(ssl, psk_client_cb);

      // Even though plain PSK doesn't authenticate the server with a
      // certificate (and therefore needs no signature), OpenSSL 3 servers
      // can run a sigalg-selection step during ClientHello processing and
      // alert with "no suitable signature algorithm" when none of the
      // client's advertised algorithms match. Pin a permissive list so a
      // PSK-only peer always finds an acceptable algorithm even though the
      // sigalg never gets used on the wire.
      SSL_set1_sigalgs_list(ssl, "RSA+SHA512:RSA+SHA384:RSA+SHA256:RSA+SHA1:ECDSA+SHA512:ECDSA+SHA384:ECDSA+SHA256:ECDSA+SHA1");

      // Verify the cipher list isn't empty after parsing. SSL_set_cipher_list
      // can return 1 (success) but leave zero ciphers enabled when the seclevel
      // strips everything or when the bundled OpenSSL build lacks the PSK
      // suites we requested. A silently empty list would let TLS proceed using
      // OpenSSL's *default* cipher list (cert-based) and fail at handshake.
      STACK_OF(SSL_CIPHER) *ciphers = SSL_get_ciphers(ssl);
      if (ciphers == nullptr || sk_SSL_CIPHER_num(ciphers) == 0) {
        throw socket_helpers::socket_exception("PSK cipher list parsed but produced zero enabled ciphers (requested: " + cipher_list +
                                               "). Check OpenSSL version / seclevel.");
      }
    }
  }

  ~nsca_ng_connection() {
    boost::system::error_code ec;
    timer_.cancel(ec);
  }

  // Run an async op against the io_service with the configured deadline.
  // The passed-in initiator should call `start_async(handler)` with a handler
  // signature of `void(boost::system::error_code)`.
  template <class Initiator>
  void run_with_deadline(Initiator initiator, const std::string &what) {
    boost::system::error_code op_ec = boost::asio::error::would_block;
    bool timed_out = false;

    timer_.expires_from_now(boost::posix_time::seconds(timeout_seconds_));
    timer_.async_wait([this, &timed_out](const boost::system::error_code &ec) {
      if (ec != boost::asio::error::operation_aborted) {
        timed_out = true;
        boost::system::error_code ignored;
        ssl_socket_.lowest_layer().cancel(ignored);
      }
    });

    initiator([&op_ec, this](const boost::system::error_code &ec) {
      op_ec = ec;
      boost::system::error_code ignored;
      timer_.cancel(ignored);
    });

    io_service_.restart();
    while (op_ec == boost::asio::error::would_block) {
      io_service_.run_one();
    }

    if (timed_out) {
      throw socket_helpers::socket_exception(what + " timed out after " + std::to_string(timeout_seconds_) + "s");
    }
    if (op_ec) {
      // boost::system::error_code::message() returns the OS-localized native
      // encoding (e.g. CP1252 on Swedish Windows). Convert to UTF-8 so the
      // string can flow through protobuf log fields without being rejected.
      throw socket_helpers::socket_exception(what + " failed: " + utf8::utf8_from_native(op_ec.message()));
    }
  }

  void connect(const std::string &host, const std::string &port, const connection_data &con, const psk_credentials &creds) {
    psk_creds_ = creds;
    SSL_set_ex_data(ssl_socket_.native_handle(), get_psk_ex_data_index(), &psk_creds_);

    const boost::asio::ip::tcp::resolver::query query(host, port);
    boost::system::error_code resolve_ec;
    auto endpoints = resolver_.resolve(query, resolve_ec);
    if (resolve_ec) throw socket_helpers::socket_exception("Failed to resolve " + host + ": " + utf8::utf8_from_native(resolve_ec.message()));

    run_with_deadline(
        [this, &endpoints](auto handler) {
          // The 2nd handler arg differs between the iterator overload
          // (resolver::iterator) and the results-type overload (tcp::endpoint);
          // accept either via auto.
          boost::asio::async_connect(ssl_socket_.lowest_layer(), endpoints, [handler](const boost::system::error_code &ec, auto /*next*/) { handler(ec); });
        },
        "connect to " + host + ":" + port);

    if (con.use_psk) {
      // With PSK the key itself authenticates both ends — no X.509 cert.
      // PSK mode intentionally ignores cert/key/ca/verify-mode/tls-version.
      ssl_socket_.set_verify_mode(boost::asio::ssl::verify_none);
    } else {
      // Apply the full cert-mode TLS configuration: certificate + key,
      // CA, verify mode, TLS min/max version, allowed ciphers, DH params.
      std::list<std::string> errors;
      con.ssl.configure_ssl_context(ctx_, errors);
      if (!errors.empty()) {
        std::string joined;
        for (const auto &e : errors) joined += (joined.empty() ? "" : "; ") + e;
        throw socket_helpers::socket_exception("TLS configuration error: " + joined);
      }

      const auto vmode = con.ssl.get_verify_mode();
      const bool verifying = (vmode & boost::asio::ssl::verify_peer) != 0;

      // Refuse to fall through to an unauthenticated TLS session unless the
      // operator has explicitly opted in. Without this the connection would
      // tunnel data through an unverified peer — vulnerable to MITM.
      if (!verifying && !con.insecure) {
        throw socket_helpers::socket_exception(
            "Refusing to connect: TLS peer verification is disabled and PSK is not in use. "
            "Either configure 'verify mode = peer-cert' with a 'ca = <path>', re-enable 'use psk = true', "
            "or set 'insecure = true' to override (disables MITM protection).");
      }

      // Hostname pinning still required even when CA verifies.
      if (verifying) {
        ssl_socket_.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));
      }
    }

    run_with_deadline(
        [this](auto handler) {
          ssl_socket_.async_handshake(boost::asio::ssl::stream_base::client, [handler](const boost::system::error_code &ec) { handler(ec); });
        },
        "TLS handshake");
    tls_active_ = true;
  }

  void write_line(const std::string &line) {
    const std::string data = line + "\n";
    write_raw(data);
  }

  void write_raw(const std::string &data) {
    run_with_deadline(
        [this, &data](auto handler) {
          boost::asio::async_write(ssl_socket_, boost::asio::buffer(data), [handler](const boost::system::error_code &ec, std::size_t /*n*/) { handler(ec); });
        },
        "write");
  }

  std::string read_line() {
    run_with_deadline(
        [this](auto handler) {
          boost::asio::async_read_until(ssl_socket_, in_buf_, '\n', [handler](const boost::system::error_code &ec, std::size_t /*n*/) { handler(ec); });
        },
        "read");
    std::istream is(&in_buf_);
    std::string line;
    std::getline(is, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
  }

  void graceful_close() {
    if (tls_active_) {
      boost::system::error_code ec;
      try {
        run_with_deadline([this](auto handler) { ssl_socket_.async_shutdown([handler](const boost::system::error_code &ec) { handler(ec); }); },
                          "TLS shutdown");
      } catch (...) {
        // Ignore; we're closing anyway.
      }
    }
    boost::system::error_code ec;
    ssl_socket_.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    ssl_socket_.lowest_layer().close(ec);
  }
};

// Build the Nagios external command for one payload entry, honouring the
// per-target host_check_default flag and a legacy "host_check" alias hack
// (preserved for backwards compatibility with existing configurations).
struct check_command_inputs {
  std::string sender_hostname;
  std::string service;  // empty -> host check
  int code{};
  std::string output;
  long timestamp{};
};

// Note on payload type: `SubmitRequestMessage::payload` is declared in
// plugin.proto as `repeated QueryResponseMessage.Response payload = 3`,
// so each element is a QueryResponseMessage::Response — there is no
// SubmitRequestMessage::Request nested type. The original implementation
// used `auto` and so never had to spell this out.
check_command_inputs make_check_inputs(const PB::Commands::QueryResponseMessage::Response &p, const connection_data &con) {
  check_command_inputs out;
  out.sender_hostname = con.sender_hostname;
  std::string alias = p.alias();
  if (alias.empty()) alias = p.command();
  out.code = nscapi::protobuf::functions::gbp_to_nagios_status(p.result());
  out.output = nscapi::protobuf::functions::query_data_to_nagios_string(p, con.max_output_length);
  out.timestamp = static_cast<long>(std::time(nullptr));

  // B4 fix: the per-target `host check = true` setting promotes every
  // submission on this target to a host check. The legacy `alias == "host_check"`
  // sentinel is also still honoured so existing configurations don't break.
  const bool is_host_check = con.host_check_default || alias == "host_check";
  out.service = is_host_check ? std::string() : alias;
  return out;
}

// One full submit transaction (connect → MOIN → PUSH each payload → QUIT).
// Returns true on success, false on a transport/protocol failure with
// `error_message` populated. Server-side rejections (FAIL/BAIL responses)
// are reported as failure too, but the caller should *not* retry those —
// the boolean retry-or-not is signalled via `retryable`.
struct submit_outcome {
  bool ok = false;
  bool retryable = false;
  std::string error_message;
};

submit_outcome do_send_once(const connection_data &con, const PB::Commands::SubmitRequestMessage &request_message) {
  submit_outcome r;
  try {
    // For PSK, allowed_ciphers (when set) overrides the default PSK cipher list.
    // For cert mode, the cipher list is applied later via configure_ssl_context.
    nsca_ng_connection conn(con.use_psk, con.use_psk ? con.ssl.allowed_ciphers : std::string(), con.timeout);
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Connecting to: " + con.to_string()); }
    psk_credentials creds{con.identity, con.password};
    conn.connect(con.get_address(), con.get_port(), con, creds);

    // MOIN handshake
    const std::string session_id = generate_session_id();
    const std::string moin_req = nsca_ng::build_moin_request(session_id);
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("C: " + moin_req); }
    conn.write_line(moin_req);

    const std::string moin_resp = conn.read_line();
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("S: " + moin_resp); }
    const auto moin_result = nsca_ng::parse_server_response(moin_resp);
    if (!moin_result.ok()) {
      r.error_message = "NSCA-NG handshake failed: " + moin_resp;
      // Server told us no — don't retry, the password is just wrong.
      r.retryable = false;
      try {
        conn.write_line("QUIT");
        (void)conn.read_line();
      } catch (...) {
      }
      conn.graceful_close();
      return r;
    }

    for (const auto &p : request_message.payload()) {
      const auto inputs = make_check_inputs(p, con);
      const std::string cmd = nsca_ng::build_check_result_command(inputs.sender_hostname, inputs.service, inputs.code, inputs.output, inputs.timestamp);
      const std::string data = cmd + "\n";
      const std::string push_req = nsca_ng::build_push_request(data.size());

      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("C: " + push_req); }
      conn.write_line(push_req);

      const std::string push_resp = conn.read_line();
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("S: " + push_resp); }
      const auto push_result = nsca_ng::parse_server_response(push_resp);
      if (!push_result.ok()) {
        r.error_message = "NSCA-NG PUSH rejected: " + push_resp;
        r.retryable = false;
        try {
          conn.write_line("QUIT");
          (void)conn.read_line();
        } catch (...) {
        }
        conn.graceful_close();
        return r;
      }

      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("C: " + cmd); }
      conn.write_raw(data);

      const std::string cmd_resp = conn.read_line();
      NSC_TRACE_ENABLED() { NSC_TRACE_MSG("S: " + cmd_resp); }
      const auto cmd_result = nsca_ng::parse_server_response(cmd_resp);
      if (!cmd_result.ok()) {
        r.error_message = "NSCA-NG submission rejected: " + cmd_resp;
        r.retryable = false;
        try {
          conn.write_line("QUIT");
          (void)conn.read_line();
        } catch (...) {
        }
        conn.graceful_close();
        return r;
      }
    }

    conn.write_line("QUIT");
    const std::string quit_resp = conn.read_line();
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("S: " + quit_resp); }
    conn.graceful_close();

    r.ok = true;
    return r;
  } catch (const socket_helpers::socket_exception &e) {
    r.error_message = std::string("NSCA-NG network error: ") + e.what();
    r.retryable = true;
    return r;
  } catch (const boost::system::system_error &e) {
    // boost::system::system_error::what() bakes in the OS-native error string;
    // convert so the message is safe to put into UTF-8 protobuf log fields.
    r.error_message = std::string("NSCA-NG error: ") + utf8::utf8_from_native(e.what());
    r.retryable = true;
    return r;
  } catch (const std::exception &e) {
    r.error_message = std::string("NSCA-NG error: ") + utf8::utf8_from_native(e.what());
    r.retryable = true;
    return r;
  } catch (...) {
    r.error_message = "Unknown NSCA-NG error -- REPORT THIS!";
    r.retryable = true;
    return r;
  }
}

void send(PB::Commands::SubmitResponseMessage::Response *payload, const connection_data &con, const PB::Commands::SubmitRequestMessage &request_message) {
  const int max_attempts = std::max(1, con.retry + 1);
  submit_outcome last;
  for (int attempt = 1; attempt <= max_attempts; ++attempt) {
    last = do_send_once(con, request_message);
    if (last.ok) {
      nscapi::protobuf::functions::set_response_good(*payload, "Submission successful");
      return;
    }
    if (!last.retryable || attempt == max_attempts) break;
    NSC_LOG_ERROR("NSCA-NG attempt " + std::to_string(attempt) + "/" + std::to_string(max_attempts) + " failed: " + last.error_message + "; retrying...");
    std::this_thread::sleep_for(std::chrono::seconds(attempt));
  }
  nscapi::protobuf::functions::set_response_bad(*payload, last.error_message);
}

}  // namespace

// =====================================================================
// nsca_ng_client_handler
// =====================================================================

bool nsca_ng_client_handler::query(client::destination_container, client::destination_container, const PB::Commands::QueryRequestMessage &,
                                   PB::Commands::QueryResponseMessage &) {
  // D3 fix: previously returned false silently. Surface it so a misuse is
  // visible at trace level rather than being a debugging mystery.
  NSC_LOG_ERROR("NSCA-NG client does not support query — only submit. Configure the request as a submit channel.");
  return false;
}

bool nsca_ng_client_handler::exec(client::destination_container, client::destination_container, const PB::Commands::ExecuteRequestMessage &,
                                  PB::Commands::ExecuteResponseMessage &) {
  NSC_LOG_ERROR("NSCA-NG client does not support exec — only submit.");
  return false;
}

bool nsca_ng_client_handler::metrics(client::destination_container, client::destination_container, const PB::Metrics::MetricsMessage &) {
  NSC_LOG_ERROR("NSCA-NG client does not support metrics push.");
  return false;
}

bool nsca_ng_client_handler::submit(client::destination_container sender, client::destination_container target,
                                    const PB::Commands::SubmitRequestMessage &request_message, PB::Commands::SubmitResponseMessage &response_message) {
  const PB::Common::Header &request_header = request_message.header();
  nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
  connection_data con(target, sender);

  // (The connection details are already traced from do_send_once via con.to_string().)

  send(response_message.add_payload(), con, request_message);
  return true;
}

}  // namespace nsca_ng_client
