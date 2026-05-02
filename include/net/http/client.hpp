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

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <bytes/base64.hpp>
#include <istream>
#include <memory>
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

struct generic_socket {
  typedef boost::asio::ip::basic_endpoint<tcp> tcp_iterator;

  virtual ~generic_socket() = default;
  virtual void connect(const std::string &server_name, const std::string &port) = 0;
  virtual void write(boost::asio::streambuf &buffer) = 0;
  virtual void read_until(boost::asio::streambuf &buffer, const std::string &until) = 0;
  virtual bool is_open() const = 0;
  virtual std::size_t read_some(boost::asio::streambuf &buffer, boost::system::error_code &error) = 0;
};

struct tcp_socket final : generic_socket {
  tcp::socket socket_;
  tcp::resolver resolver_;

  explicit tcp_socket(boost::asio::io_service &io_service) : socket_(io_service), resolver_(io_service) {}
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
    const tcp::resolver::query query(server, port);
    tcp::resolver::iterator endpoint_iterator = resolver_.resolve(query);
    const tcp::resolver::iterator end;

    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end) {
      this->connect_tcp(*endpoint_iterator, server, error);
      ++endpoint_iterator;
    }
    if (error) {
      throw socket_helpers::socket_exception("Failed to connect to " + server + ":" + port + ": " + error.message());
    }
  }

  void write(boost::asio::streambuf &buffer) override { boost::asio::write(socket_, buffer); }
  void read_until(boost::asio::streambuf &buffer, const std::string &until) override { boost::asio::read_until(socket_, buffer, until); }
  bool is_open() const override { return socket_.is_open(); }
  std::size_t read_some(boost::asio::streambuf &buffer, boost::system::error_code &error) override {
    return boost::asio::read(socket_, buffer, boost::asio::transfer_at_least(1), error);
  }
};

struct ssl_socket final : generic_socket {
  boost::asio::ssl::context context_;
  boost::asio::ssl::stream<tcp::socket> ssl_socket_;
  tcp::resolver resolver_;
  boost::asio::ssl::verify_mode verify_;
  proxy_config proxy_;

  explicit ssl_socket(boost::asio::io_service &io_service, boost::asio::ssl::context::method method, boost::asio::ssl::verify_mode verify,
                      const std::string &ca, proxy_config proxy = proxy_config())
      : context_(method), ssl_socket_(io_service, context_), resolver_(io_service), verify_(verify), proxy_(std::move(proxy)) {
    if (!ca.empty() && ca != "none") {
      try {
        context_.load_verify_file(ca);
      } catch (const std::exception &e) {
        throw socket_helpers::socket_exception("Failed to load CA " + ca + ": " + e.what());
      }
    }
  }

  ~ssl_socket() override {
    try {
      ssl_socket_.lowest_layer().close();
    } catch (...) {
    }
  }

  void connect_tcp(const tcp_iterator &endpoint_iterator, const std::string &server_name, boost::system::error_code &error) {
    ssl_socket_.lowest_layer().close();
    ssl_socket_.lowest_layer().connect(endpoint_iterator, error);

    if (error) {
      return;
    }
    ssl_socket_.set_verify_mode(verify_);
    if (!server_name.empty()) {
      SSL_set_tlsext_host_name(ssl_socket_.native_handle(), server_name.c_str());
    }
    ssl_socket_.set_verify_callback(boost::asio::ssl::rfc2818_verification(server_name));

    ssl_socket_.handshake(boost::asio::ssl::stream_base::client, error);
  }

  /// Establish an HTTP CONNECT tunnel through proxy_ then perform TLS handshake.
  void connect_via_http_proxy(const std::string &real_host, const std::string &real_port) {
    // Step 1 — TCP connect to the proxy using the underlying stream socket.
    // next_layer() returns the tcp::socket (basic_stream_socket) that supports
    // both connect() and stream I/O; lowest_layer() only gives basic_socket.
    auto &tcp_sock = ssl_socket_.next_layer();

    const tcp::resolver::query query(proxy_.host, proxy_.port);
    tcp::resolver::iterator endpoint_it = resolver_.resolve(query);
    const tcp::resolver::iterator end;

    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_it != end) {
      tcp_sock.close();
      tcp_sock.connect(*endpoint_it, error);
      ++endpoint_it;
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

    boost::asio::write(tcp_sock, boost::asio::buffer(connect_req), error);
    if (error) {
      throw socket_helpers::socket_exception("Failed to send CONNECT to proxy " + proxy_.host + ":" + proxy_.port + ": " + error.message());
    }

    // Step 3 — Read status line
    boost::asio::streambuf response_buf;
    boost::asio::read_until(tcp_sock, response_buf, "\r\n");
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
      boost::asio::read_until(tcp_sock, response_buf, "\r\n\r\n", drain_ec);
      // Best-effort read of remaining bytes (proxy typically closes after error).
      while (!drain_ec) {
        boost::asio::read(tcp_sock, response_buf, boost::asio::transfer_at_least(1), drain_ec);
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
    boost::asio::read_until(tcp_sock, response_buf, "\r\n\r\n");

    // Step 4 — TLS handshake over the established tunnel
    ssl_socket_.set_verify_mode(verify_);
    if (!real_host.empty()) {
      SSL_set_tlsext_host_name(ssl_socket_.native_handle(), real_host.c_str());
    }
    ssl_socket_.set_verify_callback(boost::asio::ssl::rfc2818_verification(real_host));
    ssl_socket_.handshake(boost::asio::ssl::stream_base::client, error);
    if (error) {
      throw socket_helpers::socket_exception("TLS handshake via proxy tunnel failed: " + error.message());
    }
  }

  void connect(const std::string &server, const std::string &port) override {
    if (proxy_.is_set() && proxy_.type == proxy_type::HTTP && !should_bypass(server, proxy_.no_proxy)) {
      connect_via_http_proxy(server, port);
      return;
    }

    const tcp::resolver::query query(server, port);
    tcp::resolver::iterator endpoint_iterator = resolver_.resolve(query);
    const tcp::resolver::iterator end;

    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end) {
      this->connect_tcp(*endpoint_iterator, server, error);
      ++endpoint_iterator;
    }
    if (error) {
      throw socket_helpers::socket_exception("Failed to connect to " + server + ":" + port + ": " + error.message());
    }
  }

  void write(boost::asio::streambuf &buffer) override { boost::asio::write(ssl_socket_, buffer); }
  void read_until(boost::asio::streambuf &buffer, const std::string &until) override { boost::asio::read_until(ssl_socket_, buffer, until); }
  bool is_open() const override { return ssl_socket_.lowest_layer().is_open(); }
  std::size_t read_some(boost::asio::streambuf &buffer, boost::system::error_code &error) override {
    return boost::asio::read(ssl_socket_, buffer, boost::asio::transfer_at_least(1), error);
  }
};
#ifdef WIN32
struct file_socket final : generic_socket {
  boost::asio::windows::stream_handle handle_;

  explicit file_socket(boost::asio::io_service &io_service) : handle_(io_service) {}
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
  proxy_config proxy_;

  http_client_options(std::string protocol, std::string tls_version, std::string verify, std::string ca, proxy_config proxy = proxy_config())
      : protocol_(std::move(protocol)), tls_version_(std::move(tls_version)), verify_(std::move(verify)), ca_(std::move(ca)), proxy_(std::move(proxy)) {}

  boost::asio::ssl::context::method get_method() const { return socket_helpers::tls_method_parser(tls_version_); }

  boost::asio::ssl::context::verify_mode get_verify() const { return socket_helpers::verify_mode_parser(verify_); };

  bool is_https() const { return protocol_ == "https"; }
  bool is_pipe() const { return protocol_ == "pipe"; }
};

class simple_client {
  boost::asio::io_service io_service_;
  std::unique_ptr<generic_socket> socket_;
  http_client_options options_;

 public:
  explicit simple_client(const http_client_options &options) : options_(options) {
    if (options.is_https()) {
      socket_ = std::make_unique<ssl_socket>(io_service_, options.get_method(), options.get_verify(), options.ca_, options.proxy_);
#ifdef WIN32
    } else if (options.is_pipe()) {
      socket_ = std::make_unique<file_socket>(io_service_);
#endif
    } else {
      socket_ = std::make_unique<tcp_socket>(io_service_);
    }
  }

  ~simple_client() = default;

  void connect(const std::string &server, const std::string &port) const { socket_->connect(server, port); }

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

  response execute(std::ostream &os, const std::string &server, const std::string &port, const request &req) {
    const bool use_proxy = options_.proxy_.is_set() && !should_bypass(server, options_.proxy_.no_proxy);

    if (use_proxy && !options_.is_https()) {
      // Plain HTTP via proxy: connect to proxy, send request with absolute URI.
      socket_->connect(options_.proxy_.host, options_.proxy_.port);
      const request proxy_req = make_proxy_request(req, server, port, options_.proxy_);
      send_request(proxy_req);
    } else {
      // Direct connect, or HTTPS (proxy CONNECT tunnel handled inside ssl_socket).
      connect(server, port);
      send_request(req);
    }

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
    connect(server, port);
    send_request(req);

    boost::asio::streambuf response_buffer;
    response resp = read_result(response_buffer);

    std::ostringstream os;
    if (response_buffer.size() > 0) os << &response_buffer;
    if (socket_->is_open()) {
      boost::system::error_code error;
      while (socket_->read_some(response_buffer, error)) {
        os << &response_buffer;
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
