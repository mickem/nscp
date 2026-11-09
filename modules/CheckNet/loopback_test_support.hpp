// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Test support for the CheckNet checks that talk to a server: a scripted
// listener on 127.0.0.1 that plays the server's half of one conversation per
// accepted connection, plus an in-process TLS identity, so a check can be run
// end to end without depending on the network the test runs on.

#include <atomic>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <cctype>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef USE_SSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <boost/asio/ssl.hpp>
#endif

namespace check_net_test {

using boost::asio::ip::tcp;

// What a scripted server does with one accepted connection. It runs on the
// server's own thread and may block: the check closes its end when it is done,
// which is what every script ultimately waits for (see drain()).
typedef std::function<void(tcp::socket &)> script_type;

// Read until the peer hangs up. Ending a script this way keeps the server's
// side of the socket open for exactly as long as the check wants it, so a
// script never races the check by closing first unless that is the point.
inline void drain(tcp::socket &s) {
  char buf[4096];
  boost::system::error_code ec;
  while (!ec) s.read_some(boost::asio::buffer(buf), ec);
}

inline void send(tcp::socket &s, const std::string &data) {
  boost::system::error_code ec;
  boost::asio::write(s, boost::asio::buffer(data), ec);
}

// Read one CRLF/LF terminated line (or whatever arrives before the peer stops).
inline std::string read_line(tcp::socket &s) {
  std::string line;
  char c = 0;
  boost::system::error_code ec;
  while (!ec) {
    if (s.read_some(boost::asio::buffer(&c, 1), ec) != 1) break;
    line += c;
    if (c == '\n') break;
  }
  return line;
}

// A listener on 127.0.0.1 that hands each of the first `scripts.size()`
// connections to the matching script. The destructor unblocks a pending accept
// by connecting to itself, so a test whose check never connects (an argument
// error) still tears the server down.
class scripted_server {
 public:
  explicit scripted_server(std::vector<script_type> scripts)
      : acceptor_(io_, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0)), scripts_(std::move(scripts)) {
    port_ = acceptor_.local_endpoint().port();
    thread_ = std::thread([this] {
      for (const script_type &script : scripts_) {
        tcp::socket s(io_);
        boost::system::error_code ec;
        acceptor_.accept(s, ec);
        if (ec || stopping_) return;
        script(s);
      }
    });
  }
  explicit scripted_server(script_type script) : scripted_server(std::vector<script_type>{std::move(script)}) {}

  ~scripted_server() {
    stopping_ = true;
    {
      // Harmless when every accept already happened: nobody is listening for it.
      boost::asio::io_context io;
      tcp::socket poke(io);
      boost::system::error_code ec;
      poke.connect(tcp::endpoint(boost::asio::ip::address_v4::loopback(), port_), ec);
    }
    if (thread_.joinable()) thread_.join();
  }

  std::string port() const { return std::to_string(port_); }

 private:
  boost::asio::io_context io_;
  tcp::acceptor acceptor_;
  std::vector<script_type> scripts_;
  unsigned short port_ = 0;
  std::atomic<bool> stopping_{false};
  std::thread thread_;
};

// A port nothing listens on: bind one, then let it go.
inline std::string closed_port() {
  boost::asio::io_context io;
  tcp::acceptor a(io, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
  const unsigned short port = a.local_endpoint().port();
  a.close();
  return std::to_string(port);
}

// The request a scripted HTTP server received: the head (request line and
// headers, as sent) and the body, read to Content-Length.
struct http_request {
  std::string head;
  std::string body;

  // The request line, e.g. "GET /path HTTP/1.1".
  std::string request_line() const { return head.substr(0, head.find("\r\n")); }
  // A header's value by case-insensitive name, empty when absent.
  std::string header(const std::string &name) const {
    std::string lower_head = head;
    std::string lower_name = name + ":";
    for (char &c : lower_head) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char &c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const std::string::size_type at = lower_head.find("\r\n" + lower_name);
    if (at == std::string::npos) return "";
    const std::string::size_type start = at + 2 + lower_name.size();
    const std::string::size_type end = head.find("\r\n", start);
    std::string value = head.substr(start, end - start);
    while (!value.empty() && value.front() == ' ') value.erase(0, 1);
    return value;
  }
};

// Read one HTTP request off the socket.
inline http_request read_http_request(tcp::socket &s) {
  http_request rq;
  std::string data;
  char buf[4096];
  boost::system::error_code ec;
  std::string::size_type end = std::string::npos;
  while (!ec && (end = data.find("\r\n\r\n")) == std::string::npos) {
    const std::size_t n = s.read_some(boost::asio::buffer(buf), ec);
    data.append(buf, n);
  }
  if (end == std::string::npos) return rq;
  rq.head = data.substr(0, end + 2);
  rq.body = data.substr(end + 4);
  const std::string length = rq.header("Content-Length");
  const std::size_t want = length.empty() ? 0 : static_cast<std::size_t>(std::stoul(length));
  while (!ec && rq.body.size() < want) {
    const std::size_t n = s.read_some(boost::asio::buffer(buf), ec);
    rq.body.append(buf, n);
  }
  return rq;
}

// A complete response with Content-Length, sent on a connection that is then
// closed - the server's half of a one-shot fetch.
inline std::string http_response(const int code, const std::string &body, const std::string &extra_headers = "") {
  return "HTTP/1.1 " + std::to_string(code) + " Status\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n" + extra_headers +
         "\r\n" + body;
}

// A script that records the request into `seen` (when given) and answers it.
inline script_type http_reply(const std::string &response, http_request *seen = nullptr) {
  return [response, seen](tcp::socket &s) {
    const http_request rq = read_http_request(s);
    if (seen) *seen = rq;
    send(s, response);
    boost::system::error_code ec;
    s.shutdown(tcp::socket::shutdown_send, ec);
    drain(s);
  };
}

#ifdef USE_SSL

// A self-signed EC certificate for CN=localhost with subjectAltName
// DNS:localhost, valid for `days` from now (negative: already expired).
struct test_identity {
  EVP_PKEY *key = nullptr;
  X509 *cert = nullptr;

  explicit test_identity(const long days = 30) {
    key = EVP_EC_gen("P-256");
    cert = X509_new();
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 4711);
    // Valid from a day before the expiry when it is already expired, so the
    // certificate is well-formed (notBefore < notAfter) either way.
    X509_gmtime_adj(X509_getm_notBefore(cert), (days < 0 ? days - 1 : -1) * 86400L);
    X509_gmtime_adj(X509_getm_notAfter(cert), days * 86400L);
    X509_NAME *name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0);
    X509_set_issuer_name(cert, name);
    X509_set_pubkey(cert, key);
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);
    X509_EXTENSION *san = X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_alt_name, "DNS:localhost");
    X509_add_ext(cert, san, -1);
    X509_EXTENSION_free(san);
    X509_sign(cert, key, EVP_sha256());
  }
  ~test_identity() {
    X509_free(cert);
    EVP_PKEY_free(key);
  }
  test_identity(const test_identity &) = delete;
  test_identity &operator=(const test_identity &) = delete;

  // The certificate as a PEM bundle on disk, for ca=.
  std::string write_pem() const {
    const boost::filesystem::path p = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-check-tcp-%%%%%%%%.pem");
    FILE *f = std::fopen(p.string().c_str(), "w");
    PEM_write_X509(f, cert);
    std::fclose(f);
    return p.string();
  }
};

// A script that performs the server side of a TLS handshake with `id`, then
// runs `then` over the encrypted stream (or just waits for the peer to leave).
typedef std::function<void(boost::asio::ssl::stream<tcp::socket &> &)> tls_script_type;

inline script_type tls_server(const test_identity &id, tls_script_type then = {}) {
  return [&id, then](tcp::socket &s) {
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tls_server);
    SSL_CTX_use_certificate(ctx.native_handle(), id.cert);
    SSL_CTX_use_PrivateKey(ctx.native_handle(), id.key);
    boost::asio::ssl::stream<tcp::socket &> stream(s, ctx);
    boost::system::error_code ec;
    stream.handshake(boost::asio::ssl::stream_base::server, ec);
    if (!ec && then) then(stream);
    drain(s);
  };
}
#endif  // USE_SSL

}  // namespace check_net_test
