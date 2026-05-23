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

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <net/socket/socket_helpers.hpp>  // for socket_helpers::extract_peer_subject_cn used in ssl_connection
#ifdef USE_SSL
#include <boost/asio/ssl/context.hpp>
#endif

#include <boost/asio/ssl/stream.hpp>
#include <list>
#include <str/utf8.hpp>
#include <str/xtos.hpp>

namespace socket_helpers {
namespace server {
using boost::asio::ip::tcp;

// SFINAE shim: call protocol.set_peer_identity(dn) when the protocol
// type defines it, otherwise do nothing. Lets ssl_connection forward
// the verified peer DN to any protocol that cares (today: NRPE's
// read_protocol) without forcing every protocol implementation to
// declare the method. The `int`/`long` overload trick disambiguates
// at compile time: the `int` version is preferred when valid, the
// `long` fallback is selected when set_peer_identity is missing.
template <typename Protocol>
auto maybe_set_peer_identity(Protocol& proto, const std::string& dn, int) -> decltype(proto.set_peer_identity(dn), void()) {
  proto.set_peer_identity(dn);
}
template <typename Protocol>
void maybe_set_peer_identity(Protocol&, const std::string&, long) {}

//
// The socket statemachine:
// This is the idea not how it currently looks
// SOCKET  | SSL-SOCKET | PROTOCOL   | RETURN
//         | connect    |            |
// connect | handhake   | on_connect | true = allow, false = disallow
// ...
//         |            | wants_data | true = yes, read chunk, false = no, start sending
// recv    | recv       | on_read    | true = read more, false = done reading
//         |            | has_data   | true = yes, send chunk, false = no, stop sending
// recv    | recv       | on_read    | true = read more, false = done reading
// ...
//         |            | is_done    | true = is done, disconnect, false (read/write loop)

template <class protocol_type, std::size_t N>
class connection : public std::enable_shared_from_this<connection<protocol_type, N> >, private boost::noncopyable {
  typedef connection<protocol_type, N> connection_type;

 public:
  connection(boost::asio::io_context& io_service, std::shared_ptr<protocol_type> protocol)
      : is_active_(true), strand_(io_service), timer_(io_service), protocol_(protocol) {}
  virtual ~connection() {}

  inline void trace(std::string msg) const {
    if (protocol_type::debug_trace) protocol_->log_debug(__FILE__, __LINE__, msg);
  }

  virtual boost::asio::ip::tcp::socket& get_socket() = 0;
  virtual bool is_open() = 0;

  //////////////////////////////////////////////////////////////////////////
  // High level connection start/stop
  virtual void start() {
    trace("start()");
    if (protocol_->on_connect()) {
      set_timeout(protocol_->get_info().timeout);
      do_process();
    } else {
      on_done(false);
    }
  }
  void cancel_socket() {
    trace("cancel_socket()");
    boost::system::error_code ignored_ec;
    if (is_open()) {
      try {
        trace("socket.shutdown()");
        get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
        if (is_open()) get_socket().close(ignored_ec);
      } catch (...) {
      }
    }
  }

  virtual void on_done(bool all_ok) {
    is_active_ = false;

    trace(std::string("on_done(") + (all_ok ? "true" : "false") + ")");
    try {
      cancel_timer();
      cancel_socket();
    } catch (...) {
      protocol_->log_error(__FILE__, __LINE__, "Failed to close connection");
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Timeout related functions
  virtual void set_timeout(int seconds) {
    timer_.expires_from_now(boost::posix_time::seconds(seconds));
    auto self(this->shared_from_this());
    timer_.async_wait([self](const auto& e) { self->timeout(e); });
  }

  virtual void cancel_timer() {
    try {
      trace("cancel_timer()");
      timer_.cancel();
    } catch (...) {
    }
  }

  virtual void timeout(const boost::system::error_code& e) {
    if (e != boost::asio::error::operation_aborted) {
      trace("timeout()");
      on_done(false);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Socket state machine (assumed all sockets are simple connect-read-write-disconnect

  void do_process() {
    trace("s - do_process()");
    try {
      if (protocol_->wants_data()) {
        if (is_active_) start_read_request();
      } else if (protocol_->has_data()) {
        trace("s - has_data() == true");
        if (!is_open()) {
          protocol_->log_error(__FILE__, __LINE__, "Socket was unexpectedly closed trying to send data (possibly check your timeout settings)");
          on_done(false);
          return;
        }
        if (is_active_) start_write_request(buf(protocol_->get_outbound()));
      } else {
        if (is_active_) on_done(true);
      }
    } catch (const std::exception& e) {
      protocol_->log_error(__FILE__, __LINE__, "Protocol error: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      protocol_->log_error(__FILE__, __LINE__, "Protocol error: UNKNOWN");
    }
  }

  virtual void start_read_request() = 0;
  virtual void handle_read_request(const boost::system::error_code& e, std::size_t bytes_transferred) {
    trace("handle_read_request(" + utf8::utf8_from_native(e.message()) + ", " + str::xtos(bytes_transferred) + ")");
    if (!e) {
      if (protocol_->on_read(buffer_.begin(), buffer_.begin() + bytes_transferred)) {
        do_process();
      } else {
        on_done(false);
      }
    } else {
      protocol_->log_debug(__FILE__, __LINE__, "Failed to read data: " + utf8::utf8_from_native(e.message()));
      on_done(false);
    }
  }

  virtual void start_write_request(const boost::asio::const_buffer& response) = 0;
  virtual void handle_write_response(const boost::system::error_code& e, std::size_t bytes_transferred) {
    trace("handle_write_response(" + utf8::utf8_from_native(e.message()) + ", " + str::xtos(bytes_transferred) + ")");
    if (!e) {
      protocol_->on_write();
      do_process();
    } else {
      // Distinguish "client gave up before we could reply" from generic write
      // failures. The classic case: an upstream check_nrpe / firewall / load
      // balancer cut the connection before NSClient++ finished computing the
      // response. Without this hint operators see only the OS-level error
      // (e.g. "broken pipe", "connection reset by peer") and have no way to
      // tell that the check itself completed but the answer landed on a
      // closed socket. The check process is NOT terminated by this — see
      // CheckExternalScripts `timeout=` for that.
      const auto v = e.value();
      if (v == boost::asio::error::broken_pipe || v == boost::asio::error::connection_reset || v == boost::asio::error::operation_aborted ||
          v == boost::asio::error::eof) {
        protocol_->log_error(__FILE__, __LINE__,
                             "Client disconnected before NSClient++ could send the response (" + utf8::utf8_from_native(e.message()) +
                                 "). The check ran to completion server-side; the upstream likely hit its own timeout. "
                                 "Bytes written before disconnect: " +
                                 str::xtos(bytes_transferred));
      } else {
        protocol_->log_error(__FILE__, __LINE__, "Failed to send data: " + utf8::utf8_from_native(e.message()));
      }
      on_done(false);
    }
  }

 protected:
  //////////////////////////////////////////////////////////////////////////
  // Internal functions and data

  boost::asio::const_buffer buf(const typename protocol_type::outbound_buffer_type s) {
    buffers_.push_back(s);
    return boost::asio::buffer(buffers_.back());
  }

  bool is_active_;
  boost::asio::io_context::strand strand_;
  boost::array<char, N> buffer_;
  boost::asio::deadline_timer timer_;
  std::list<typename protocol_type::outbound_buffer_type> buffers_;
  std::shared_ptr<protocol_type> protocol_;
};

template <class protocol_type, std::size_t N>
class tcp_connection : public connection<protocol_type, N> {
 private:
  typedef connection<protocol_type, N> parent_type;
  typedef tcp_connection<protocol_type, N> my_type;

  tcp::socket socket_;

 public:
  tcp_connection(boost::asio::io_context& io_service, std::shared_ptr<protocol_type> protocol)
      : connection<protocol_type, N>(io_service, protocol), socket_(io_service) {}
  ~tcp_connection() override = default;

  bool is_open() override { return socket_.is_open(); }
  tcp::socket& get_socket() override { return socket_; }

  void start_read_request() override {
    this->trace("tcp::start_read_request()");
    auto self(this->shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(parent_type::buffer_),
        boost::asio::bind_executor(parent_type::strand_, [self](const auto& e, auto bytes_transferred) { self->handle_read_request(e, bytes_transferred); }));
  }
  void start_write_request(const boost::asio::const_buffer& response) override {
    this->trace("start_write_request(" + str::xtos(boost::asio::buffer_size(response)) + ")");
    auto self(this->shared_from_this());
    boost::asio::async_write(socket_, response, boost::asio::bind_executor(parent_type::strand_, [self](const auto& e, auto bytes_transferred) {
                               self->handle_write_response(e, bytes_transferred);
                             }));
  }
};

#ifdef USE_SSL
template <class protocol_type, std::size_t N>
class ssl_connection : public connection<protocol_type, N> {
  typedef connection<protocol_type, N> parent_type;
  typedef ssl_connection<protocol_type, N> my_type;

 public:
  ssl_connection(boost::asio::io_context& io_service, boost::asio::ssl::context& context, std::shared_ptr<protocol_type> protocol)
      : connection<protocol_type, N>(io_service, protocol), ssl_socket_(io_service, context) {}
  ~ssl_connection() override = default;

  bool is_open() override { return get_socket().is_open(); }

  tcp::socket& get_socket() override { return ssl_socket_.next_layer(); }

  void start() override {
    this->trace("ssl::start_read_request()");
    std::shared_ptr<my_type> self = std::dynamic_pointer_cast<my_type>(this->shared_from_this());
    ssl_socket_.async_handshake(boost::asio::ssl::stream_base::server,
                                boost::asio::bind_executor(parent_type::strand_, [self](const auto& e) { self->handle_handshake(e); }));
  }

  virtual void handle_handshake(const boost::system::error_code& e) {
    if (!e) {
      // Extract the verified peer Subject CN before continuing. The boost
      // SSL stream's native_handle() returns the SSL* whose handshake
      // completed under the configured verify_mode - if the server
      // requires `peer,fail-if-no-peer-cert`, the handshake would not be
      // here unless the cert was both presented and chain-validated. If
      // verify_mode is `none`, the CN we extract is attacker-supplied;
      // callers (NRPEServer) must enforce the verify_mode requirement
      // separately before treating the CN as identity.
      //
      // CN-only (not full RFC 2253 DN) because the principal ends up as
      // part of an INI policy key like `NRPEServer:icinga-master`, and
      // INI key syntax uses `=` as the key/value separator - a DN like
      // `CN=icinga-master,O=Acme` would be split mid-key by simpleini.
      // If an identity-map indirection lands later, the connection layer
      // can stamp the full DN and the map can resolve it to a handle.
      try {
        const std::string cn = socket_helpers::extract_peer_subject_cn(ssl_socket_.native_handle());
        if (!cn.empty()) {
          parent_type::protocol_->log_debug(__FILE__, __LINE__, "TLS peer Subject CN: " + cn);
        }
        maybe_set_peer_identity(*parent_type::protocol_, cn, 0);
      } catch (...) {
        // Extraction failure must not break the connection - the
        // handshake itself succeeded, so we still want to serve the
        // request. The protocol just won't see an identity.
        parent_type::protocol_->log_error(__FILE__, __LINE__, "Failed to extract peer Subject CN after TLS handshake");
      }
      parent_type::start();
    } else {
      const int reason = ERR_GET_REASON(e.value());
      if (reason == SSL_R_NO_SHARED_CIPHER) {
        parent_type::protocol_->log_error(__FILE__, __LINE__, "Seems we cant agree on SSL: " + utf8::utf8_from_native(e.message()));
        parent_type::protocol_->log_error(__FILE__, __LINE__, "Please review the insecure options as well as ssl options in settings.");
      } else if (reason == SSL_R_UNKNOWN_PROTOCOL) {
        parent_type::protocol_->log_error(__FILE__, __LINE__, "Seems we other end is not using ssl: " + utf8::utf8_from_native(e.message()));
        parent_type::protocol_->log_error(__FILE__, __LINE__, "Please review the ssl option as well as ssl options in settings.");
      } else if (reason == SSL_R_CERTIFICATE_VERIFY_FAILED) {
        parent_type::protocol_->log_error(__FILE__, __LINE__, "Failed to verify other ends certificate: " + utf8::utf8_from_native(e.message()));
        parent_type::protocol_->log_error(__FILE__, __LINE__, "Please review the ssl option as well as ssl options in settings.");
      } else {
        parent_type::protocol_->log_error(__FILE__, __LINE__,
                                          "Failed to establish secure connection: " + utf8::utf8_from_native(e.message()) + ": " + str::xtos(reason));
      }
      parent_type::on_done(false);
    }
  }

  void start_read_request() override {
    this->trace("ssl::start_read_request()");
    auto self(this->shared_from_this());
    ssl_socket_.async_read_some(
        boost::asio::buffer(parent_type::buffer_),
        boost::asio::bind_executor(parent_type::strand_, [self](const auto& e, auto bytes_transferred) { self->handle_read_request(e, bytes_transferred); }));
  }

  void start_write_request(const boost::asio::const_buffer& response) override {
    this->trace("ssl::start_write_request(" + str::xtos(boost::asio::buffer_size(response)) + ")");
    auto self(this->shared_from_this());
    boost::asio::async_write(ssl_socket_, response, boost::asio::bind_executor(parent_type::strand_, [self](const auto& e, auto bytes_transferred) {
                               self->handle_write_response(e, bytes_transferred);
                             }));
  }

 protected:
  typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
  ssl_socket ssl_socket_;
};
#endif
}  // namespace server
}  // namespace socket_helpers
