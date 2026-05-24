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
#include <boost/optional.hpp>
#include <chrono>
#include <memory>
#include <net/socket/allowed_hosts.hpp>
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
// Extract the peer certificate's Subject DN from an established SSL
// session and format it as an RFC 2253 string (e.g.
// `CN=icinga-master,O=Acme,C=US`). Returns an empty string when there is
// no peer cert (one-way TLS), the formatter fails, or `ssl` is null.
// Used by ssl_connection after the handshake to surface the verified
// caller identity to higher-level protocol handlers (NRPEServer, ...).
//
// IMPORTANT: only call after `verify_mode` includes `peer` *and*
// `fail-if-no-peer-cert` with a `ca path` pointing at the trusted
// issuer. Otherwise the returned DN is attacker-supplied and must NOT
// be used for authorization decisions.
std::string extract_peer_subject_dn(void* ssl);

// Format an X509 certificate's Subject as an RFC 2253 DN string.
// Exposed for unit testing (so tests can construct an X509 in memory
// without standing up a TLS session) and for callers that already
// have an X509 in hand. Returns empty when `x509` is null or when the
// underlying X509_NAME_print_ex fails. The `void*` is the X509 pointer
// from OpenSSL - taking it as void* keeps openssl/x509.h out of this
// header's transitive include set.
//
// NOTE: the resulting DN contains `=` between attribute and value
// (`CN=host,O=Acme`). This survives in-memory just fine but does NOT
// round-trip through an INI key (simpleini splits the line on the
// first `=`), so DN strings cannot be used as policy keys today.
// Prefer extract_peer_subject_cn / format_subject_cn_only when the
// result will be written to nsclient.ini as a settings key.
std::string format_subject_dn_rfc2253(void* x509);

// Extract just the CN (common name) value from the peer certificate's
// Subject. Returns empty when there is no peer cert, no CN entry, or
// `ssl` is null. Used for the `client identity source = cn` mode, which
// is the safe choice for INI-stored permission policies because the
// resulting principal contains no `=` and round-trips through the
// settings store unchanged.
std::string extract_peer_subject_cn(void* ssl);

// Format-only counterpart to extract_peer_subject_cn. Returns the CN
// value of `x509`'s Subject, or empty when no CN entry is present.
// Same void* / openssl-isolation convention as
// format_subject_dn_rfc2253. Exposed for unit testing.
std::string format_subject_cn_only(void* x509);
#endif
void validate_certificate(const std::string& certificate, std::list<std::string>& list);

// Resolve a hostname spec used by the various submit-clients.
//   "auto"     -> system host name as-is
//   "auto-lc"  -> system host name, lower-cased
//   "auto-uc"  -> system host name, upper-cased
//   anything else: ${host}, ${domain}, ${host_lc}, ${host_uc}, ${domain_lc}
//   and ${domain_uc} are substituted from the system host name (split on the
//   first '.' into host and domain). Other text is preserved.
std::string expand_hostname(std::string spec);

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

struct timed_writer : std::enable_shared_from_this<timed_writer> {
  boost::asio::io_context& io_service;
  boost::asio::steady_timer timer;

  boost::optional<boost::system::error_code> timer_result;
  boost::optional<boost::system::error_code> read_result;

  explicit timed_writer(boost::asio::io_context& io_service) : io_service(io_service), timer(io_service) {}
  ~timed_writer() {
    // cancel() can throw, and an exception escaping a destructor risks
    // std::terminate during stack unwinding. The non-throwing cancel(ec)
    // overload is deprecated/removed under BOOST_ASIO_NO_DEPRECATED, so we use
    // the throwing overload and swallow any error here.
    try {
      timer.cancel();
    } catch (...) {
    }
  }
  void start_timer(const std::chrono::milliseconds duration) {
    timer.expires_after(duration);
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
    io_service.restart();
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
bool write_with_timeout(boost::asio::io_context& io_service, AsyncWriteStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers,
                        const std::chrono::milliseconds duration) {
  boost::optional<boost::system::error_code> timer_result;
  boost::asio::steady_timer timer(io_service);
  timer.expires_after(duration);
  timer.async_wait([&timer_result](const auto& e) { set_result(&timer_result, e); });

  boost::optional<boost::system::error_code> read_result;
  async_write(sock, buffers, [&read_result](const auto& e) { set_result(&read_result, e); });

  io_service.restart();
  while (io_service.run_one()) {
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

struct timed_reader : std::enable_shared_from_this<timed_reader> {
  boost::asio::io_context& io_service;
  std::chrono::milliseconds duration;
  boost::asio::steady_timer timer;

  boost::optional<boost::system::error_code> timer_result;
  boost::optional<boost::system::error_code> write_result;

  explicit timed_reader(boost::asio::io_context& io_service) : io_service(io_service), duration(0), timer(io_service) {}
  ~timed_reader() {
    // cancel() can throw, and an exception escaping a destructor risks
    // std::terminate during stack unwinding. The non-throwing cancel(ec)
    // overload is deprecated/removed under BOOST_ASIO_NO_DEPRECATED, so we use
    // the throwing overload and swallow any error here.
    try {
      timer.cancel();
    } catch (...) {
    }
  }

  void start_timer(const std::chrono::milliseconds duration_) {
    timer.expires_after(duration_);
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
    io_service.restart();
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
bool read_with_timeout(boost::asio::io_context& io_service, AsyncReadStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers,
                       const std::chrono::milliseconds duration) {
  boost::optional<boost::system::error_code> timer_result;
  boost::asio::steady_timer timer(io_service);
  timer.expires_after(duration);
  timer.async_wait([&timer_result](const auto& e) { set_result(&timer_result, e); });

  boost::optional<boost::system::error_code> read_result;
  async_read(sock, buffers, [&read_result](const auto& e) { set_result(&read_result, e); });

  io_service.restart();
  while (io_service.run_one()) {
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

  if (read_result && *read_result) throw boost::system::system_error(*read_result);
  return false;
}
}  // namespace io
}  // namespace socket_helpers
