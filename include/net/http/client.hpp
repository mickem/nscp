// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/asio.hpp>
#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif
#include <bytes/base64.hpp>
#include <istream>
#include <memory>
#include <net/address_family.hpp>
#include <net/http/http_packet.hpp>
#include <net/http/proxy_config.hpp>
#include <net/socket/socket_helpers.hpp>
#include <ostream>
#include <sstream>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utility>
#include <vector>

using boost::asio::ip::tcp;

namespace http {

// Decode an HTTP/1.1 chunked-transfer-encoded body. Each chunk is preceded by
// a hex chunk-size line ("6f\r\n"), optionally followed by a chunk-extension
// after a ';' which is ignored, then the chunk data and a CRLF. The body ends
// with a zero-size chunk ("0\r\n\r\n"), optionally followed by trailer
// headers. Returns the decoded payload; on malformed input returns whatever
// has been decoded successfully so far rather than throwing.
inline std::string decode_chunked(const std::string &raw) {
  std::string out;
  out.reserve(raw.size());
  std::size_t pos = 0;
  while (pos < raw.size()) {
    const std::size_t crlf = raw.find("\r\n", pos);
    if (crlf == std::string::npos) break;
    std::string size_line = raw.substr(pos, crlf - pos);
    const auto semi = size_line.find(';');
    if (semi != std::string::npos) size_line.erase(semi);
    while (!size_line.empty() && std::isspace(static_cast<unsigned char>(size_line.back()))) size_line.pop_back();
    std::size_t size = 0;
    try {
      size = std::stoul(size_line, nullptr, 16);
    } catch (...) {
      break;
    }
    pos = crlf + 2;
    if (size == 0) break;
    if (pos + size > raw.size()) {
      // Truncated chunk: append what we have and stop.
      out.append(raw, pos, raw.size() - pos);
      break;
    }
    out.append(raw, pos, size);
    pos += size;
    // Skip the CRLF that terminates each chunk's data.
    if (pos + 1 < raw.size() && raw[pos] == '\r' && raw[pos + 1] == '\n') pos += 2;
  }
  return out;
}

struct parsed_url {
  std::string protocol;
  std::string host;
  std::string port;
  std::string path;
};

inline parsed_url parse_url(const std::string &url) {
  parsed_url result;
  const std::string sep = "://";
  const auto sep_pos = url.find(sep);
  if (sep_pos == std::string::npos) return result;
  result.protocol = url.substr(0, sep_pos);
  const std::string rest = url.substr(sep_pos + sep.size());
  const auto slash_pos = rest.find('/');
  const std::string hostport = (slash_pos != std::string::npos) ? rest.substr(0, slash_pos) : rest;
  result.path = (slash_pos != std::string::npos) ? rest.substr(slash_pos) : "/";
  const auto colon_pos = hostport.find(':');
  if (colon_pos != std::string::npos) {
    result.host = hostport.substr(0, colon_pos);
    result.port = hostport.substr(colon_pos + 1);
  } else {
    result.host = hostport;
    result.port = (result.protocol == "https") ? "443" : "80";
  }
  return result;
}

// Bound how long a single read or write may take. Every transfer here is
// synchronous, so a peer that accepts the connection and then stops talking
// blocks the calling thread indefinitely - for a background loop like the
// fleet agent that means the host silently stops being managed until the
// service restarts.
//
// SO_RCVTIMEO does NOT achieve this: Asio's synchronous operations treat the
// resulting EAGAIN as "would block" and then wait on the descriptor with no
// deadline of their own. The operation therefore has to be issued
// asynchronously and the io_context run with a deadline.
//
// Zero (the default) keeps the historical behaviour of waiting forever, so
// only callers that opt in change at all.
template <typename Operation, typename CancelTarget>
std::size_t run_with_deadline(boost::asio::io_context &io, CancelTarget &cancel_target, const unsigned int seconds, const char *what,
                              boost::system::error_code &ec, Operation start) {
  std::size_t transferred = 0;
  bool completed = false;
  start([&](const boost::system::error_code &result, const std::size_t bytes) {
    ec = result;
    transferred = bytes;
    completed = true;
  });
  io.restart();
  io.run_for(std::chrono::seconds(seconds));
  if (!completed) {
    // Deadline hit: cancel the outstanding operation and let its handler run
    // so the io_context is left clean for the next call.
    boost::system::error_code ignored;
    cancel_target.cancel(ignored);
    io.restart();
    io.run();
    ec = boost::asio::error::timed_out;
    throw socket_helpers::socket_exception(std::string(what) + " timed out after " + str::xtos(seconds) + "s");
  }
  return transferred;
}

struct generic_socket {
  typedef boost::asio::ip::basic_endpoint<tcp> tcp_iterator;

  virtual ~generic_socket() = default;
  virtual void connect(const std::string &server_name, const std::string &port) = 0;
  virtual void write(boost::asio::streambuf &buffer) = 0;
  virtual void read_until(boost::asio::streambuf &buffer, const std::string &until) = 0;
  virtual bool is_open() const = 0;
  virtual std::size_t read_some(boost::asio::streambuf &buffer, boost::system::error_code &error) = 0;
  // Days until the peer's TLS certificate expires (negative if already
  // expired); none when the transport is not TLS or no peer certificate is
  // available. The long form collapses none to -1 for legacy callers that
  // predate the distinction.
  virtual boost::optional<long> peer_certificate_expiry_days_opt() const { return boost::none; }
  long peer_certificate_expiry_days() const { return peer_certificate_expiry_days_opt().get_value_or(-1); }
  // Applied by the client right after connecting; no-op for transports that
  // are not sockets.
  virtual void set_timeouts(unsigned int seconds) {}
  // Pins which IP version the connection uses; no-op for transports that do not
  // resolve host names (named pipes).
  virtual void set_address_family(net::address_family af) {}
};

struct tcp_socket final : generic_socket {
  tcp::socket socket_;
  tcp::resolver resolver_;
  boost::asio::io_context &io_;
  unsigned int timeout_ = 0;
  net::address_family address_family_ = net::address_family::any;

  void set_address_family(const net::address_family af) override { address_family_ = af; }

  explicit tcp_socket(boost::asio::io_context &io_service) : socket_(io_service), resolver_(io_service), io_(io_service) {}
  ~tcp_socket() override {
    try {
      socket_.close();
    } catch (...) {
    }
  }

  void connect_tcp(const tcp_iterator &endpoint_iterator, const std::string &_server_name, boost::system::error_code &error) {
    socket_.close();
    socket_.connect(endpoint_iterator, error);
  }

  void connect(const std::string &server, const std::string &port) override {
    boost::system::error_code resolve_ec;
    auto endpoints = net::resolve_for_family(resolver_, address_family_, server, port, resolve_ec);
    if (resolve_ec || endpoints.begin() == endpoints.end()) {
      throw socket_helpers::socket_exception("Failed to resolve " + server + ":" + port + " over " + net::to_string(address_family_) + ": " +
                                             (resolve_ec ? resolve_ec.message() : std::string("no address in the requested family")));
    }

    boost::system::error_code error = boost::asio::error::host_not_found;
    for (auto it = endpoints.begin(); error && it != endpoints.end(); ++it) {
      this->connect_tcp(it->endpoint(), server, error);
    }
    if (error) {
      throw socket_helpers::socket_exception("Failed to connect to " + server + ":" + port + ": " + error.message());
    }
  }

  void write(boost::asio::streambuf &buffer) override {
    if (timeout_ == 0) {
      boost::asio::write(socket_, buffer);
      return;
    }
    boost::system::error_code ec;
    run_with_deadline(io_, socket_, timeout_, "Write", ec, [&](auto handler) { boost::asio::async_write(socket_, buffer, handler); });
    if (ec) throw socket_helpers::socket_exception("Failed to send request: " + ec.message());
  }
  void read_until(boost::asio::streambuf &buffer, const std::string &until) override {
    if (timeout_ == 0) {
      boost::asio::read_until(socket_, buffer, until);
      return;
    }
    boost::system::error_code ec;
    run_with_deadline(io_, socket_, timeout_, "Read", ec, [&](auto handler) { boost::asio::async_read_until(socket_, buffer, until, handler); });
    if (ec) throw socket_helpers::socket_exception("Failed to read response: " + ec.message());
  }
  bool is_open() const override { return socket_.is_open(); }
  std::size_t read_some(boost::asio::streambuf &buffer, boost::system::error_code &error) override {
    if (timeout_ == 0) {
      return boost::asio::read(socket_, buffer, boost::asio::transfer_at_least(1), error);
    }
    // The body drain treats any error as end of body, so a deadline here ends
    // the response instead of throwing - the caller then fails to parse a
    // truncated payload, which is a far better outcome than never returning.
    try {
      return run_with_deadline(io_, socket_, timeout_, "Read", error,
                               [&](auto handler) { boost::asio::async_read(socket_, buffer, boost::asio::transfer_at_least(1), handler); });
    } catch (const socket_helpers::socket_exception &) {
      error = boost::asio::error::timed_out;
      return 0;
    }
  }
  void set_timeouts(const unsigned int seconds) override { timeout_ = seconds; }
};

#ifndef WIN32
// Unix domain socket transport: what the "pipe" protocol means on non-Windows
// platforms (e.g. the docker daemon socket /var/run/docker.sock). The `server`
// argument is the socket path; `port` is ignored. Windows uses file_socket
// (named pipes) for the same protocol instead.
struct unix_socket final : generic_socket {
  boost::asio::local::stream_protocol::socket socket_;
  boost::asio::io_context &io_;
  unsigned int timeout_ = 0;

  explicit unix_socket(boost::asio::io_context &io_service) : socket_(io_service), io_(io_service) {}
  ~unix_socket() override {
    try {
      socket_.close();
    } catch (...) {
    }
  }

  void connect(const std::string &server, const std::string &) override {
    boost::system::error_code error;
    socket_.connect(boost::asio::local::stream_protocol::endpoint(server), error);
    if (error) {
      throw socket_helpers::socket_exception("Failed to connect to " + server + ": " + error.message());
    }
  }

  void write(boost::asio::streambuf &buffer) override {
    if (timeout_ == 0) {
      boost::asio::write(socket_, buffer);
      return;
    }
    boost::system::error_code ec;
    run_with_deadline(io_, socket_, timeout_, "Write", ec, [&](auto handler) { boost::asio::async_write(socket_, buffer, handler); });
    if (ec) throw socket_helpers::socket_exception("Failed to send request: " + ec.message());
  }
  void read_until(boost::asio::streambuf &buffer, const std::string &until) override {
    if (timeout_ == 0) {
      boost::asio::read_until(socket_, buffer, until);
      return;
    }
    boost::system::error_code ec;
    run_with_deadline(io_, socket_, timeout_, "Read", ec, [&](auto handler) { boost::asio::async_read_until(socket_, buffer, until, handler); });
    if (ec) throw socket_helpers::socket_exception("Failed to read response: " + ec.message());
  }
  bool is_open() const override { return socket_.is_open(); }
  std::size_t read_some(boost::asio::streambuf &buffer, boost::system::error_code &error) override {
    if (timeout_ == 0) {
      return boost::asio::read(socket_, buffer, boost::asio::transfer_at_least(1), error);
    }
    // See tcp_socket::read_some: a deadline here ends the body instead of
    // throwing, so the caller fails on a truncated payload rather than hanging.
    try {
      return run_with_deadline(io_, socket_, timeout_, "Read", error,
                               [&](auto handler) { boost::asio::async_read(socket_, buffer, boost::asio::transfer_at_least(1), handler); });
    } catch (const socket_helpers::socket_exception &) {
      error = boost::asio::error::timed_out;
      return 0;
    }
  }
  void set_timeouts(const unsigned int seconds) override { timeout_ = seconds; }
};
#endif

// In-memory TLS material for mutual TLS: a client certificate + key presented
// to the server, and optionally a pinned server certificate used as the only
// trust root (instead of a CA file / system store). All fields are PEM
// strings, not paths - callers like the fleet agent keep this material in a
// state file rather than as loose certificate files.
struct client_identity {
  std::string cert_pem;       // client certificate (chain) to present
  std::string key_pem;        // matching private key
  std::string pinned_ca_pem;  // if set: the ONLY trusted root for the peer; hostname verification is skipped

  bool has_client_cert() const { return !cert_pem.empty() && !key_pem.empty(); }
  bool is_pinned() const { return !pinned_ca_pem.empty(); }
  bool empty() const { return !has_client_cert() && !is_pinned(); }
};

// Encode an ALPN protocol list (RFC 7301) the way OpenSSL wants it: each name
// prefixed by its own length as a single byte, all concatenated - not a
// NUL-separated or comma-separated string. Kept out of the TLS code (and out of
// the USE_SSL guard) so the encoding can be tested on its own.
//
// A name must fit in that length byte and cannot be empty; both are programming
// errors rather than anything a peer controls, so they throw rather than being
// silently dropped - a quietly missing protocol shows up as a confusing
// handshake or certificate error much later.
inline std::string alpn_wire_format(const std::vector<std::string> &protocols) {
  std::string wire;
  for (const std::string &protocol : protocols) {
    if (protocol.empty()) throw socket_helpers::socket_exception("Invalid ALPN protocol: empty name");
    if (protocol.size() > 255) throw socket_helpers::socket_exception("Invalid ALPN protocol (longer than 255 bytes): " + protocol);
    wire.push_back(static_cast<char>(static_cast<unsigned char>(protocol.size())));
    wire.append(protocol);
  }
  return wire;
}

#ifdef USE_SSL
struct ssl_socket final : generic_socket {
  boost::asio::ssl::context context_;
  boost::asio::ssl::stream<tcp::socket> ssl_socket_;
  tcp::resolver resolver_;
  boost::asio::ssl::verify_mode verify_;
  net::address_family address_family_ = net::address_family::any;
  std::string sni_;  // TLS SNI / verification hostname override (empty = use the connected host)
  proxy_config proxy_;
  bool pinned_;  // pinned peer cert: verify against it only, no hostname check
  boost::asio::io_context &io_;
  unsigned int timeout_ = 0;

  // Build the fully-configured TLS context BEFORE any SSL stream exists. OpenSSL's
  // SSL_new() COPIES the certificate/key state out of the context at creation time
  // (only the verify store is shared by reference), so a client certificate loaded
  // into the context after the stream is constructed is silently ignored: the
  // handshake then presents no certificate at all and an mTLS server answers with
  // a bare "certificate required" alert. Keep every use_certificate/use_private_key
  // call in here, never in the ssl_socket constructor body. The same applies to
  // the ALPN list: SSL_new copies it out of the context too.
  static boost::asio::ssl::context make_context(const boost::asio::ssl::context::method method, const std::string &ca, const client_identity &identity,
                                                const boost::asio::ssl::verify_mode verify, const std::vector<std::string> &alpn = std::vector<std::string>(),
                                                const long tls_min_version = 0) {
    // Fail closed: presenting a client certificate while neither pinning the
    // server nor verifying it is unauthenticated mTLS - the client credential
    // would be handed to whatever server answers, including a man in the middle.
    // No caller should be able to construct that combination by accident. The
    // fleet agent relies on the pin (guaranteed non-empty by load_state), so it
    // never trips this; a truncated identity or a future caller might, and must
    // be stopped here rather than silently downgraded.
    if (identity.has_client_cert() && !identity.is_pinned() && (verify & boost::asio::ssl::verify_peer) == 0) {
      throw socket_helpers::socket_exception(
          "Refusing an mTLS connection with no server authentication: a client certificate was supplied without a pinned server certificate "
          "and with peer verification disabled. Pin the server certificate or enable verification.");
    }
    boost::asio::ssl::context context(method);
    // A "1.2+" tls version resolves to the generic TLS method plus a floor;
    // the floor is not part of the method, so it must be applied here or the
    // '+' silently degrades to "any".
    if (tls_min_version != 0 && SSL_CTX_set_min_proto_version(context.native_handle(), tls_min_version) != 1) {
      throw socket_helpers::socket_exception("Failed to set the minimum TLS version");
    }
    if (!ca.empty() && ca != "none") {
      try {
        context.load_verify_file(ca);
      } catch (const std::exception &e) {
        throw socket_helpers::socket_exception("Failed to load CA " + ca + ": " + e.what());
      }
    }
    if (identity.is_pinned()) {
      try {
        context.add_certificate_authority(boost::asio::buffer(identity.pinned_ca_pem));
      } catch (const std::exception &e) {
        throw socket_helpers::socket_exception(std::string("Failed to load pinned server certificate: ") + e.what());
      }
    }
    if (identity.has_client_cert()) {
      try {
        context.use_certificate_chain(boost::asio::buffer(identity.cert_pem));
        context.use_private_key(boost::asio::buffer(identity.key_pem), boost::asio::ssl::context::pem);
      } catch (const std::exception &e) {
        throw socket_helpers::socket_exception(std::string("Failed to load client certificate/key: ") + e.what());
      }
    }
    if (!alpn.empty()) {
      const std::string wire = alpn_wire_format(alpn);
      // Note the inverted convention: SSL_CTX_set_alpn_protos returns 0 on
      // success. It only fails on allocation, but a silently unset list would
      // surface as whatever the server does to a client that offered nothing -
      // for the fleet mux, the public web certificate and a pin failure.
      if (SSL_CTX_set_alpn_protos(context.native_handle(), reinterpret_cast<const unsigned char *>(wire.data()), static_cast<unsigned int>(wire.size())) != 0)
        throw socket_helpers::socket_exception("Failed to set the ALPN protocol list");
    }
    return context;
  }

  explicit ssl_socket(boost::asio::io_context &io_service, boost::asio::ssl::context::method method, boost::asio::ssl::verify_mode verify,
                      const std::string &ca, std::string sni = std::string(), proxy_config proxy = proxy_config(),
                      const client_identity &identity = client_identity(), const std::vector<std::string> &alpn = std::vector<std::string>(),
                      const long tls_min_version = 0)
      : context_(make_context(method, ca, identity, verify, alpn, tls_min_version)),
        ssl_socket_(io_service, context_),
        resolver_(io_service),
        verify_(identity.is_pinned() ? boost::asio::ssl::verify_peer : verify),  // the pin IS the peer identity: always verify against it
        sni_(std::move(sni)),
        proxy_(std::move(proxy)),
        pinned_(identity.is_pinned()),
        io_(io_service) {}

  void set_address_family(const net::address_family af) override { address_family_ = af; }

  ~ssl_socket() override {
    try {
      ssl_socket_.lowest_layer().close();
    } catch (...) {
    }
  }

  // The TLS handshake is I/O like any read or write: a peer that accepts the
  // TCP connection and then stalls mid-handshake would otherwise wedge the
  // calling thread forever, so it gets the same deadline treatment.
  void handshake(boost::system::error_code &error) {
    if (timeout_ == 0) {
      ssl_socket_.handshake(boost::asio::ssl::stream_base::client, error);
      return;
    }
    try {
      run_with_deadline(io_, ssl_socket_.lowest_layer(), timeout_, "TLS handshake", error, [&](auto handler) {
        ssl_socket_.async_handshake(boost::asio::ssl::stream_base::client, [handler](const boost::system::error_code &ec) { handler(ec, 0); });
      });
    } catch (const socket_helpers::socket_exception &) {
      error = boost::asio::error::timed_out;
    }
  }

  void connect_tcp(const tcp_iterator &endpoint_iterator, const std::string &server_name, boost::system::error_code &error) {
    ssl_socket_.lowest_layer().close();
    ssl_socket_.lowest_layer().connect(endpoint_iterator, error);

    if (error) {
      return;
    }
    const std::string tls_name = sni_.empty() ? server_name : sni_;
    ssl_socket_.set_verify_mode(verify_);
    if (!tls_name.empty()) {
      SSL_set_tlsext_host_name(ssl_socket_.native_handle(), tls_name.c_str());
    }
    // A pinned peer certificate is the identity check itself; its subject
    // rarely matches the host we dialed, so hostname verification is skipped.
    if (!pinned_) {
      ssl_socket_.set_verify_callback(boost::asio::ssl::host_name_verification(tls_name));
    }

    handshake(error);
  }

  boost::optional<long> peer_certificate_expiry_days_opt() const override {
    // native_handle() is non-const; the underlying SSL* is not mutated here.
    SSL *ssl = const_cast<ssl_socket *>(this)->ssl_socket_.native_handle();
    return socket_helpers::peer_certificate_expiry_days(ssl);
  }

  /// Establish an HTTP CONNECT tunnel through proxy_ then perform TLS handshake.
  void connect_via_http_proxy(const std::string &real_host, const std::string &real_port) {
    // Step 1 — TCP connect to the proxy using the underlying stream socket.
    // next_layer() returns the tcp::socket (basic_stream_socket) that supports
    // both connect() and stream I/O; lowest_layer() only gives basic_socket.
    auto &tcp_sock = ssl_socket_.next_layer();

    boost::system::error_code error = boost::asio::error::host_not_found;
    auto proxy_endpoints = net::resolve_for_family(resolver_, address_family_, proxy_.host, proxy_.port, error);
    if (error) {
      throw socket_helpers::socket_exception("Failed to resolve proxy " + proxy_.host + ":" + proxy_.port + ": " + error.message());
    }
    error = boost::asio::error::host_not_found;
    for (auto it = proxy_endpoints.begin(); error && it != proxy_endpoints.end(); ++it) {
      tcp_sock.close();
      tcp_sock.connect(it->endpoint(), error);
    }
    if (error) {
      throw socket_helpers::socket_exception("Failed to connect to proxy " + proxy_.host + ":" + proxy_.port + ": " + error.message());
    }

    // Step 2 — Send CONNECT request
    std::string connect_req = "CONNECT " + real_host + ":" + real_port + " HTTP/1.0\r\n" + "Host: " + real_host + ":" + real_port + "\r\n";
    if (!proxy_.credentials().empty()) {
      connect_req += "Proxy-Authorization: Basic " + bytes::base64_encode(proxy_.credentials()) + "\r\n";
    }
    connect_req += "\r\n";

    // The CONNECT exchange is I/O against a peer like any read or write: a
    // proxy that accepts the TCP connection and then stalls must hit the same
    // deadline as the rest of the transfer, not wedge the calling thread.
    if (timeout_ == 0) {
      boost::asio::write(tcp_sock, boost::asio::buffer(connect_req), error);
    } else {
      run_with_deadline(io_, tcp_sock, timeout_, "Proxy CONNECT write", error,
                        [&](auto handler) { boost::asio::async_write(tcp_sock, boost::asio::buffer(connect_req), handler); });
    }
    if (error) {
      throw socket_helpers::socket_exception("Failed to send CONNECT to proxy " + proxy_.host + ":" + proxy_.port + ": " + error.message());
    }

    // Step 3 — Read status line
    boost::asio::streambuf response_buf;
    if (timeout_ == 0) {
      boost::asio::read_until(tcp_sock, response_buf, "\r\n");
    } else {
      boost::system::error_code read_ec;
      run_with_deadline(io_, tcp_sock, timeout_, "Proxy CONNECT read", read_ec,
                        [&](auto handler) { boost::asio::async_read_until(tcp_sock, response_buf, "\r\n", handler); });
      if (read_ec) {
        throw socket_helpers::socket_exception("Failed to read CONNECT response from proxy " + proxy_.host + ":" + proxy_.port + ": " + read_ec.message());
      }
    }
    std::istream response_stream(&response_buf);
    std::string http_version;
    unsigned int status_code = 0;
    std::string status_message;
    response_stream >> http_version >> status_code;
    std::getline(response_stream, status_message);

    if (status_code == 407 || status_code < 200 || status_code >= 300) {
      // Drain remaining headers + any body the proxy supplied so we can include
      // a snippet in the exception message — a 407 body often explains *why*
      // (realm, scheme, "user 'alice' is unknown", etc.).
      boost::system::error_code drain_ec;
      // Best-effort: the drain only enriches the error message, so a timeout
      // here just ends the drain rather than replacing the real failure.
      try {
        if (timeout_ == 0) {
          boost::asio::read_until(tcp_sock, response_buf, "\r\n\r\n", drain_ec);
        } else {
          run_with_deadline(io_, tcp_sock, timeout_, "Proxy CONNECT drain", drain_ec,
                            [&](auto handler) { boost::asio::async_read_until(tcp_sock, response_buf, "\r\n\r\n", handler); });
        }
        // Read any remaining bytes (proxy typically closes after error).
        while (!drain_ec) {
          if (timeout_ == 0) {
            boost::asio::read(tcp_sock, response_buf, boost::asio::transfer_at_least(1), drain_ec);
          } else {
            run_with_deadline(io_, tcp_sock, timeout_, "Proxy CONNECT drain", drain_ec,
                              [&](auto handler) { boost::asio::async_read(tcp_sock, response_buf, boost::asio::transfer_at_least(1), handler); });
          }
        }
      } catch (const socket_helpers::socket_exception &) {
        // Deadline hit mid-drain: report what we have.
      }
      std::string proxy_body((std::istreambuf_iterator<char>(&response_buf)), std::istreambuf_iterator<char>());
      // Strip the headers section if present so the snippet is just the body.
      const auto header_end = proxy_body.find("\r\n\r\n");
      if (header_end != std::string::npos) proxy_body.erase(0, header_end + 4);
      // Cap the snippet length to keep error messages readable.
      static const std::size_t kMaxSnippet = 256;
      if (proxy_body.size() > kMaxSnippet) proxy_body.resize(kMaxSnippet);

      const std::string proxy_label = proxy_.host + ":" + proxy_.port;
      if (status_code == 407) {
        std::string msg = "Proxy authentication required (407) for " + proxy_label;
        if (!proxy_body.empty()) msg += " — " + proxy_body;
        throw socket_helpers::socket_exception(msg);
      }
      std::string msg = "Proxy CONNECT failed with status " + str::xtos(status_code) + ": " + status_message + " (proxy: " + proxy_label + ")";
      if (!proxy_body.empty()) msg += " — " + proxy_body;
      throw socket_helpers::socket_exception(msg);
    }

    // Drain remaining proxy response headers
    if (timeout_ == 0) {
      boost::asio::read_until(tcp_sock, response_buf, "\r\n\r\n");
    } else {
      boost::system::error_code header_ec;
      run_with_deadline(io_, tcp_sock, timeout_, "Proxy CONNECT header read", header_ec,
                        [&](auto handler) { boost::asio::async_read_until(tcp_sock, response_buf, "\r\n\r\n", handler); });
      if (header_ec) {
        throw socket_helpers::socket_exception("Failed to read CONNECT response from proxy " + proxy_.host + ":" + proxy_.port + ": " + header_ec.message());
      }
    }

    // Step 4 — TLS handshake over the established tunnel
    const std::string tls_name = sni_.empty() ? real_host : sni_;
    ssl_socket_.set_verify_mode(verify_);
    if (!tls_name.empty()) {
      SSL_set_tlsext_host_name(ssl_socket_.native_handle(), tls_name.c_str());
    }
    if (!pinned_) {
      ssl_socket_.set_verify_callback(boost::asio::ssl::host_name_verification(tls_name));
    }
    handshake(error);
    if (error) {
      throw socket_helpers::socket_exception("TLS handshake via proxy tunnel failed: " + error.message());
    }
  }

  void connect(const std::string &server, const std::string &port) override {
    if (proxy_.is_set() && proxy_.type == proxy_type::HTTP && !should_bypass(server, proxy_.no_proxy)) {
      connect_via_http_proxy(server, port);
      return;
    }

    boost::system::error_code error = boost::asio::error::host_not_found;
    auto endpoints = net::resolve_for_family(resolver_, address_family_, server, port, error);
    if (error || endpoints.begin() == endpoints.end()) {
      throw socket_helpers::socket_exception("Failed to resolve " + server + ":" + port + " over " + net::to_string(address_family_) + ": " +
                                             (error ? error.message() : std::string("no address in the requested family")));
    }
    error = boost::asio::error::host_not_found;
    for (auto it = endpoints.begin(); error && it != endpoints.end(); ++it) {
      this->connect_tcp(it->endpoint(), server, error);
    }
    if (error) {
      throw socket_helpers::socket_exception("Failed to connect to " + server + ":" + port + ": " + error.message());
    }
  }

  void write(boost::asio::streambuf &buffer) override {
    if (timeout_ == 0) {
      boost::asio::write(ssl_socket_, buffer);
      return;
    }
    boost::system::error_code ec;
    run_with_deadline(io_, ssl_socket_.lowest_layer(), timeout_, "Write", ec, [&](auto handler) { boost::asio::async_write(ssl_socket_, buffer, handler); });
    if (ec) throw socket_helpers::socket_exception("Failed to send request: " + ec.message());
  }
  void read_until(boost::asio::streambuf &buffer, const std::string &until) override {
    if (timeout_ == 0) {
      boost::asio::read_until(ssl_socket_, buffer, until);
      return;
    }
    boost::system::error_code ec;
    run_with_deadline(io_, ssl_socket_.lowest_layer(), timeout_, "Read", ec,
                      [&](auto handler) { boost::asio::async_read_until(ssl_socket_, buffer, until, handler); });
    if (ec) throw socket_helpers::socket_exception("Failed to read response: " + ec.message());
  }
  void set_timeouts(const unsigned int seconds) override { timeout_ = seconds; }
  bool is_open() const override { return ssl_socket_.lowest_layer().is_open(); }
  std::size_t read_some(boost::asio::streambuf &buffer, boost::system::error_code &error) override {
    if (timeout_ == 0) {
      return boost::asio::read(ssl_socket_, buffer, boost::asio::transfer_at_least(1), error);
    }
    // As for plain TCP: a deadline ends the body rather than throwing.
    try {
      return run_with_deadline(io_, ssl_socket_.lowest_layer(), timeout_, "Read", error,
                               [&](auto handler) { boost::asio::async_read(ssl_socket_, buffer, boost::asio::transfer_at_least(1), handler); });
    } catch (const socket_helpers::socket_exception &) {
      error = boost::asio::error::timed_out;
      return 0;
    }
  }
};
#endif  // USE_SSL
#ifdef WIN32
struct file_socket final : generic_socket {
  boost::asio::windows::stream_handle handle_;

  explicit file_socket(boost::asio::io_context &io_service) : handle_(io_service) {}
  ~file_socket() override {
    try {
      handle_.close();
    } catch (...) {
    }
  }

  void connect(const std::string &pipe_name, const std::string &port) override {
    const HANDLE hPipe = ::CreateFileA(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                       FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);

    if (hPipe == INVALID_HANDLE_VALUE) {
      throw socket_helpers::socket_exception("Failed to open pipe " + pipe_name);
    }

    // assign the pipe to our handle
    handle_.assign(hPipe);
  }
  void write(boost::asio::streambuf &buffer) override { boost::asio::write(handle_, buffer); }
  void read_until(boost::asio::streambuf &buffer, const std::string &until) override { boost::asio::read_until(handle_, buffer, until); }
  bool is_open() const override { return handle_.is_open(); }
  std::size_t read_some(boost::asio::streambuf &buffer, boost::system::error_code &error) override {
    return boost::asio::read(handle_, buffer, boost::asio::transfer_at_least(1), error);
  }
};
#endif
struct http_client_options {
  std::string protocol_;
  std::string tls_version_;
  std::string verify_;
  std::string ca_;
  std::string sni_;  // optional TLS SNI / verification hostname override (empty = use the connected host)
  proxy_config proxy_;
  client_identity identity_;  // optional mutual-TLS material (in-memory PEM)
  // ALPN protocols to offer, most preferred first; empty offers none (the
  // default - an ordinary HTTP/1.1 client has nothing to negotiate). Set it
  // when the peer routes on ALPN rather than merely reporting it: the fleet
  // server shares one port between the agent API and the operator web UI and
  // picks the certificate, and whether to ask for a client certificate, from
  // the ClientHello. See onboarding::kAgentAlpn.
  std::vector<std::string> alpn_protocols_;
  // Deadline for a single read or write, in seconds; 0 waits forever. Set it
  // on any call made from a long-lived thread that must not be wedged by an
  // unresponsive peer (see set_socket_timeouts).
  unsigned int timeout_seconds_ = 0;
  // Largest response body fetch() will accumulate, in bytes; 0 is unlimited.
  // fetch() buffers the whole body in memory (twice, while it is copied out of
  // the stream), so without a limit a hostile or broken server can exhaust the
  // process. 5 MB is far above any API response we consume and far below what
  // hurts. execute() streams to the caller's ostream instead of buffering, so
  // it is deliberately not covered - that is the path large downloads use.
  std::size_t max_response_bytes_ = 5u * 1024u * 1024u;
  // Pins the connection (and the proxy connection, when one is used) to one IP
  // version; `any` leaves the choice to the resolver.
  net::address_family address_family_ = net::address_family::any;

  http_client_options(std::string protocol, std::string tls_version, std::string verify, std::string ca, proxy_config proxy = proxy_config())
      : protocol_(std::move(protocol)), tls_version_(std::move(tls_version)), verify_(std::move(verify)), ca_(std::move(ca)), proxy_(std::move(proxy)) {}

#ifdef USE_SSL
  boost::asio::ssl::context::method get_method() const { return socket_helpers::tls_method_parser(tls_version_); }

  // The minimum protocol version a "1.2+" tls version asks for (0 when the
  // version carries no floor); applied to the TLS context alongside the method.
  long get_tls_min_version() const { return socket_helpers::tls_min_version_parser(tls_version_); }

  boost::asio::ssl::context::verify_mode get_verify() const { return socket_helpers::verify_mode_parser(verify_); };
#endif  // USE_SSL

  bool is_https() const { return protocol_ == "https"; }
  bool is_pipe() const { return protocol_ == "pipe"; }
};

class simple_client {
  boost::asio::io_context io_service_;
  std::unique_ptr<generic_socket> socket_;
  http_client_options options_;

 public:
  explicit simple_client(const http_client_options &options) : options_(options) {
    if (options.is_https()) {
#ifdef USE_SSL
      socket_ = std::make_unique<ssl_socket>(io_service_, options.get_method(), options.get_verify(), options.ca_, options.sni_, options.proxy_,
                                             options.identity_, options.alpn_protocols_, options.get_tls_min_version());
#else
      throw socket_helpers::socket_exception("HTTPS requested but this build has no TLS support (compiled without OpenSSL)");
#endif
#ifdef WIN32
    } else if (options.is_pipe()) {
      socket_ = std::make_unique<file_socket>(io_service_);
#else
    } else if (options.is_pipe()) {
      // On non-Windows "pipe" means a unix domain socket (the server argument
      // is the socket path). Previously this fell through to tcp_socket, which
      // tried to DNS-resolve the path and could never connect.
      socket_ = std::make_unique<unix_socket>(io_service_);
#endif
    } else {
      socket_ = std::make_unique<tcp_socket>(io_service_);
    }
    socket_->set_address_family(options.address_family_);
  }

  ~simple_client() = default;

  void connect(const std::string &server, const std::string &port) const {
    // Before connect, not after: for TLS the handshake happens inside
    // connect(), and it must run under the same deadline as reads/writes.
    socket_->set_timeouts(options_.timeout_seconds_);
    socket_->connect(server, port);
  }

  void send_request(const request &req) const {
    boost::asio::streambuf requestbuf;
    std::ostream request_stream(&requestbuf);
    req.build_request(request_stream);
    socket_->write(requestbuf);
  }
  // Read more bytes from the underlying socket into the supplied buffer.
  // Useful for callers that want to drain a response body after read_result()
  // without going through execute() (which throws on non-2xx responses).
  std::size_t read_some(boost::asio::streambuf &buf, boost::system::error_code &ec) const { return socket_->read_some(buf, ec); }
  bool is_open() const { return socket_ && socket_->is_open(); }
  // Days until the peer TLS certificate expires; none for non-TLS or if unavailable.
  boost::optional<long> peer_certificate_expiry_days_opt() const {
    return socket_ ? socket_->peer_certificate_expiry_days_opt() : boost::optional<long>();
  }
  long peer_certificate_expiry_days() const { return peer_certificate_expiry_days_opt().get_value_or(-1); }

  response read_result(boost::asio::streambuf &response_buffer) const {
    std::string http_version, status_message;
    unsigned int status_code = 0;
    socket_->read_until(response_buffer, "\r\n");

    std::istream response_stream(&response_buffer);
    if (!response_stream) throw socket_helpers::socket_exception("Invalid response");
    response_stream >> http_version;
    response_stream >> status_code;
    std::getline(response_stream, status_message);

    response ret(http_version, status_code, status_message);

    if (ret.http_version_.substr(0, 5) != "HTTP/") throw socket_helpers::socket_exception("Invalid response: " + ret.http_version_);

    try {
      socket_->read_until(response_buffer, "\r\n\r\n");
    } catch (const std::exception &e) {
      throw socket_helpers::socket_exception(std::string("Failed to read header: ") + e.what());
    }

    std::string header;
    while (std::getline(response_stream, header) && header != "\r") ret.add_header(header);
    return ret;
  }

  /// Build a copy of request suitable for sending through an HTTP proxy.
  /// The path is rewritten to absolute-URI form and a Proxy-Authorization
  /// header is added when the proxy carries credentials.
  static request make_proxy_request(const request &original, const std::string &server, const std::string &port, const proxy_config &proxy) {
    request p = original;
    std::string abs_path = "http://" + server;
    if (!port.empty() && port != "80") abs_path += ":" + port;
    abs_path += original.path_;
    p.path_ = abs_path;
    if (!proxy.credentials().empty()) {
      p.add_header("Proxy-Authorization", "Basic " + bytes::base64_encode(proxy.credentials()));
    }
    return p;
  }

  /// Connect and send the request, routing through the configured proxy when
  /// one applies: plain HTTP goes to the proxy with an absolute-URI request,
  /// HTTPS tunnels via CONNECT inside ssl_socket. Shared by execute() and
  /// fetch() so both honour the proxy and the configured timeout on every
  /// path (connect() applies set_timeouts, including for the proxy leg).
  void connect_and_send(const std::string &server, const std::string &port, const request &req) {
    const bool use_proxy = options_.proxy_.is_set() && !should_bypass(server, options_.proxy_.no_proxy);

    if (use_proxy && !options_.is_https()) {
      connect(options_.proxy_.host, options_.proxy_.port);
      send_request(make_proxy_request(req, server, port, options_.proxy_));
    } else {
      connect(server, port);
      send_request(req);
    }
  }

  response execute(std::ostream &os, const std::string &server, const std::string &port, const request &req) {
    connect_and_send(server, port, req);

    boost::asio::streambuf response_buffer;
    const response resp = read_result(response_buffer);

    if (!resp.is_2xx()) {
      throw socket_helpers::socket_exception("Failed to " + req.verb_ + " " + server + ":" + port + " " + str::xtos(resp.status_code_) + ": " +
                                             resp.status_message_);
    }
    if (response_buffer.size() > 0) os << &response_buffer;

    if (socket_->is_open()) {
      boost::system::error_code error;
      while (socket_->read_some(response_buffer, error)) {
        os << &response_buffer;
      }
    }

    return resp;
  }

  // Like execute() but does NOT throw on non-2xx responses.
  // Populates response.payload_ with the response body.
  // Only throws on connection or protocol errors.
  response fetch(const std::string &server, const std::string &port, const request &req) {
    connect_and_send(server, port, req);

    boost::asio::streambuf response_buffer;
    response resp = read_result(response_buffer);

    // Bail out as soon as the limit is passed rather than reading to the end
    // and checking afterwards: the point is to never hold the oversized body.
    const std::size_t limit = options_.max_response_bytes_;
    const auto check_limit = [&limit, &server, &port](const std::size_t size) {
      if (limit != 0 && size > limit) {
        throw socket_helpers::socket_exception("Response from " + server + ":" + port + " exceeds the maximum size of " + str::xtos(limit) + " bytes");
      }
    };

    std::ostringstream os;
    if (response_buffer.size() > 0) os << &response_buffer;
    check_limit(static_cast<std::size_t>(os.tellp()));
    if (socket_->is_open()) {
      boost::system::error_code error;
      while (socket_->read_some(response_buffer, error)) {
        os << &response_buffer;
        check_limit(static_cast<std::size_t>(os.tellp()));
      }
    }
    resp.payload_ = os.str();

    // Servers that send Transfer-Encoding: chunked frame the body as
    // "<hex-size>\r\n<bytes>\r\n...0\r\n\r\n". Decode it here so callers see
    // the message body rather than the framing. response::add_header lower-
    // cases the key on storage, so a direct map lookup is sufficient.
    const auto te_it = resp.headers_.find("transfer-encoding");
    if (te_it != resp.headers_.end()) {
      // The header may list multiple codings (e.g. "gzip, chunked"); we only
      // handle plain "chunked". Detect it as the last token rather than a
      // substring so a content type like "x-chunked-stream" wouldn't match.
      const std::string &v = te_it->second;
      const auto last_comma = v.rfind(',');
      std::string last_coding = (last_comma == std::string::npos) ? v : v.substr(last_comma + 1);
      const auto a = last_coding.find_first_not_of(" \t");
      const auto b = last_coding.find_last_not_of(" \t");
      if (a != std::string::npos) last_coding = last_coding.substr(a, b - a + 1);
      std::transform(last_coding.begin(), last_coding.end(), last_coding.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (last_coding == "chunked") resp.payload_ = decode_chunked(resp.payload_);
    }
    return resp;
  }

  static bool download(std::string protocol, const std::string &server, const std::string &port, std::string path, std::string tls_version,
                       std::string verify_mode, std::string ca, std::ostream &os, std::string &error_msg, const proxy_config &proxy = proxy_config()) {
    try {
      request rq("GET", server, std::move(path));
      const http_client_options options(std::move(protocol), std::move(tls_version), std::move(verify_mode), std::move(ca), proxy);
      simple_client c(options);
      c.execute(os, server, port, rq);
      return true;
    } catch (const socket_helpers::socket_exception &e) {
      error_msg = e.reason();
      return false;
    } catch (const std::exception &e) {
      error_msg = std::string("Exception: ") + utf8::utf8_from_native(e.what());
      return false;
    }
  }
};
}  // namespace http
