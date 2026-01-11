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
#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>
#include <socket/allowed_hosts.hpp>
#include <str/xtos.hpp>
#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#endif

#include <list>
#include <string>
#include <utility>

namespace socket_helpers {
#ifdef USE_SSL
void write_certs(const std::string& cert, bool ca);
#endif
void validate_certificate(const std::string& certificate, std::list<std::string>& list);

class socket_exception : public std::exception {
  std::string error;

 public:
  //////////////////////////////////////////////////////////////////////////
  /// Constructor takes an error message.
  /// @param error the error message
  ///
  /// @author mickem
  explicit socket_exception(std::string error) noexcept : error(std::move(error)) {}
  socket_exception(const socket_exception& other) noexcept : socket_exception(other.reason()) {}
  ~socket_exception() noexcept override = default;

  //////////////////////////////////////////////////////////////////////////
  /// Retrieve the error message from the exception.
  /// @return the error message
  ///
  /// @author mickem
  const char* what() const noexcept override { return error.c_str(); }
  std::string reason() const { return error; }
};

struct connection_info {
  struct ssl_opts {
    ssl_opts() : enabled(false), debug_verify(false), tls_version("1.2+") {}

    ssl_opts(const ssl_opts& other)
        : enabled(other.enabled),
          debug_verify(other.debug_verify),
          certificate(other.certificate),
          certificate_format(other.certificate_format),
          certificate_key(other.certificate_key),
          ca_path(other.ca_path),
          allowed_ciphers(other.allowed_ciphers),
          dh_key(other.dh_key),
          verify_mode(other.verify_mode),
          tls_version(other.tls_version),
          ssl_options(other.ssl_options) {}
    ssl_opts& operator=(const ssl_opts& other) {
      enabled = other.enabled;
      debug_verify = other.debug_verify;
      certificate = other.certificate;
      certificate_format = other.certificate_format;
      certificate_key = other.certificate_key;
      ca_path = other.ca_path;
      allowed_ciphers = other.allowed_ciphers;
      dh_key = other.dh_key;
      verify_mode = other.verify_mode;
      tls_version = other.tls_version;
      ssl_options = other.ssl_options;
      return *this;
    }

    bool enabled;
    bool debug_verify;
    std::string certificate;
    std::string certificate_format;
    std::string certificate_key;
    std::string certificate_key_format;

    std::string ca_path;
    std::string allowed_ciphers;
    std::string dh_key;

    std::string verify_mode;
    std::string tls_version;
    std::string ssl_options;

    std::string to_string() const {
      std::stringstream ss;
      if (enabled) {
        if (debug_verify) {
          ss << "debug verify: on, ";
        }
        ss << "ssl enabled: " << verify_mode;
        if (!certificate.empty())
          ss << ", cert: " << certificate << " (" << certificate_format << "), " << certificate_key;
        else
          ss << ", no certificate";
        ss << ", dh: " << dh_key << ", ciphers: " << allowed_ciphers << ", ca: " << ca_path;
        ss << ", options: " << ssl_options;
        ss << ", tls version: " << tls_version;
      } else
        ss << "ssl disabled";
      return ss.str();
    }
#ifdef USE_SSL
    void configure_ssl_context(boost::asio::ssl::context& context, std::list<std::string>& errors) const;
    boost::asio::ssl::context::verify_mode get_verify_mode() const;
    long get_tls_min_version() const;
    long get_tls_max_version() const;
    boost::asio::ssl::context::file_format get_certificate_format() const;
    boost::asio::ssl::context::file_format get_certificate_key_format() const;
    long get_ctx_opts() const;
#endif
  };

  static const int backlog_default;
  std::string address;
  int back_log;
  std::string port_;
  unsigned int thread_pool_size;
  unsigned int timeout;
  int retry;
  bool reuse;
  ssl_opts ssl;
  allowed_hosts_manager allowed_hosts;

  connection_info() : back_log(backlog_default), port_("0"), thread_pool_size(0), timeout(30), retry(2), reuse(true) {}

  connection_info(const connection_info& other)
      : address(other.address),
        back_log(other.back_log),
        port_(other.port_),
        thread_pool_size(other.thread_pool_size),
        timeout(other.timeout),
        retry(other.retry),
        reuse(other.reuse),
        ssl(other.ssl),
        allowed_hosts(other.allowed_hosts) {}
  connection_info& operator=(const connection_info& other) {
    address = other.address;
    back_log = other.back_log;
    port_ = other.port_;
    thread_pool_size = other.thread_pool_size;
    timeout = other.timeout;
    retry = other.retry;
    reuse = other.reuse;
    ssl = other.ssl;
    allowed_hosts = other.allowed_hosts;
    return *this;
  }

  std::list<std::string> validate_ssl() const;
  std::list<std::string> validate() const;

  bool get_reuse() const { return reuse; }
  std::string get_port() const { return port_; }
  unsigned short get_int_port() const { return str::stox<unsigned short>(port_); }
  std::string get_address() const { return address; }
  std::string get_endpoint_string() const { return address + ":" + get_port(); }
  long get_ctx_opts() const;

  std::string to_string() const {
    std::stringstream ss;
    ss << "address: " << get_endpoint_string();
    ss << ", " << ssl.to_string();
    return ss.str();
  }
};
#ifdef USE_SSL
boost::asio::ssl::context_base::method tls_method_parser(const std::string& tls_version);
boost::asio::ssl::verify_mode verify_mode_parser(const std::string& verify_mode);
#endif

namespace io {
void set_result(boost::optional<boost::system::error_code>* a, const boost::system::error_code& b);

struct timed_writer : public boost::enable_shared_from_this<timed_writer> {
  boost::asio::io_service& io_service;
  // boost::posix_time::time_duration duration;
  boost::asio::deadline_timer timer;

  boost::optional<boost::system::error_code> timer_result;
  boost::optional<boost::system::error_code> read_result;

  timed_writer(boost::asio::io_service& io_service) : io_service(io_service), timer(io_service) {}
  ~timed_writer() { timer.cancel(); }
  void start_timer(boost::posix_time::time_duration duration) {
    timer.expires_from_now(duration);
    auto self(shared_from_this());
    timer.async_wait([self](const auto& e) { self->set_result(&self->timer_result, e); });
  }
  void stop_timer() { timer.cancel(); }

  template <typename AsyncWriteStream, typename MutableBufferSequence>
  void write(AsyncWriteStream& stream, MutableBufferSequence& buffer) {
    auto self(shared_from_this());
    async_write(stream, buffer, [self](const auto& e) { self->set_result(&self->read_result, e); });
  }

  template <typename AsyncWriteStream, typename Socket, typename MutableBufferSequence>
  bool write_and_wait(AsyncWriteStream& stream, Socket& socket, const MutableBufferSequence& buffer) {
    write(stream, buffer);
    return wait(socket);
  }

  template <typename Socket>
  bool wait(Socket& socket) {
    io_service.reset();
    while (io_service.run_one()) {
      if (read_result) {
        read_result.reset();
        return true;
      } else if (timer_result) {
        socket.close();
        return false;
      }
    }
    return false;
  }

  void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code ec) {
    if (!ec) a->reset(ec);
  }
};

template <typename AsyncWriteStream, typename RawSocket, typename MutableBufferSequence>
bool write_with_timeout(AsyncWriteStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
  boost::optional<boost::system::error_code> timer_result;
  boost::asio::deadline_timer timer(sock.get_io_service());
  timer.expires_from_now(duration);
  timer.async_wait([&timer_result](const auto& e) { set_result(&timer_result, e); });

  boost::optional<boost::system::error_code> read_result;
  async_write(sock, buffers, [&read_result](const auto& e) { set_result(&read_result, e); });

  sock.get_io_service().reset();
  while (sock.get_io_service().run_one()) {
    if (read_result) {
      timer.cancel();
      return true;
    } else if (timer_result) {
      rawSocket.close();
      return false;
    }
  }

  if (read_result && *read_result) throw boost::system::system_error(*read_result);
  return false;
}

struct timed_reader : public boost::enable_shared_from_this<timed_reader> {
  boost::asio::io_service& io_service;
  boost::posix_time::time_duration duration;
  boost::asio::deadline_timer timer;

  boost::optional<boost::system::error_code> timer_result;
  boost::optional<boost::system::error_code> write_result;

  timed_reader(boost::asio::io_service& io_service) : io_service(io_service), timer(io_service) {}
  ~timed_reader() { timer.cancel(); }

  void start_timer(boost::posix_time::time_duration duration_) {
    timer.expires_from_now(duration_);
    auto self(shared_from_this());
    timer.async_wait([self](const auto& e) { self->set_result(&self->timer_result, e); });
  }
  void stop_timer() { timer.cancel(); }

  template <typename AsyncWriteStream, typename MutableBufferSequence>
  void read(AsyncWriteStream& stream, const MutableBufferSequence& buffers) {
    auto self(shared_from_this());
    async_read(stream, buffers, [self](const auto& e) { self->set_result(&self->write_result, e); });
  }

  template <typename AsyncWriteStream, typename Socket, typename MutableBufferSequence>
  bool read_and_wait(AsyncWriteStream& stream, Socket& socket, const MutableBufferSequence& buffers) {
    read(stream, buffers);
    return wait(socket);
  }
  template <typename Socket>
  bool wait(Socket& socket) {
    io_service.reset();
    while (io_service.run_one()) {
      if (write_result) {
        write_result.reset();
        return true;
      } else if (timer_result) {
        socket.close();
        return false;
      }
    }
    return false;
  }
  void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code ec) {
    if (!ec) a->reset(ec);
  }
};

template <typename AsyncReadStream, typename RawSocket, typename MutableBufferSequence>
bool read_with_timeout(AsyncReadStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration) {
  boost::optional<boost::system::error_code> timer_result;
  boost::asio::deadline_timer timer(sock.get_io_service());
  timer.expires_from_now(duration);
  timer.async_wait([&timer_result](const auto& e) { set_result(&timer_result, e); });

  boost::optional<boost::system::error_code> read_result;
  async_read(sock, buffers, [&read_result](const auto& e) { set_result(&read_result, e); });

  sock.get_io_service().reset();
  while (sock.get_io_service().run_one()) {
    if (read_result) {
      timer.cancel();
      return true;
    } else if (timer_result) {
      rawSocket.close();
      return false;
    } else {
      // 					if (!rawSocket.is_open()) {
      // 						timer.cancel();
      // 						rawSocket.close();
      // 						return false;
      // 					}
    }
  }

  if (*read_result) throw boost::system::system_error(*read_result);
  return false;
}
}  // namespace io
}  // namespace socket_helpers
