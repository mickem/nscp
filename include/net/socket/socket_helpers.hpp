// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
// Generate a self-signed certificate and write it to `cert`.
//
// For `ca == false` this is the agent's own identity: the unencrypted private
// key and the certificate go into the one file (asio loads both from it when
// `certificate key` is empty), created so that only the account running the
// agent can read it.
//
// For `ca == true` the certificate goes to `cert` - the file operators hand
// out to clients - and the CA private key to ca_key_path(cert) beside it,
// again readable only by us. Distributing the CA key would let any recipient
// mint client certificates and walk through `verify mode = peer-cert`.
void write_certs(const std::string& cert, bool ca);

// Where write_certs() puts the private key belonging to a CA certificate:
// the same directory and extension with a "-key" suffix on the stem, so
// `.../ca.pem` becomes `.../ca-key.pem`.
std::string ca_key_path(const std::string& ca_certificate);

#ifdef WIN32
// Lock a file down to LOCAL SYSTEM and the local Administrators group,
// breaking inheritance so a permissive parent (Program Files grants
// `Users: Read & Execute`) cannot leave a private key world-readable.
// Mirrors nsclient::windows_acl::protect_directory, which is not linked into
// the modules that generate certificates.
bool restrict_to_owner(const std::string& path, std::list<std::string>& errors);
#endif
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

// Substitute the host name placeholders in `spec`: ${hostname}, ${hostname_lc}
// and ${hostname_uc} are the system host name as reported; ${host}, ${domain},
// ${host_lc}, ${host_uc}, ${domain_lc} and ${domain_uc} are substituted from it
// after splitting on the first '.' into host and domain. Other text is
// preserved.
//
// The machine's addresses are available too (#349): ${address_ipv4} is the
// IPv4 address, and ${address_ipv6} the IPv6 address in its RFC 5952 canonical
// form (lowercase, compressed). The IPv6 variants ${address_ipv6_lc} /
// ${address_ipv6_uc} pick the case, and appending _comp / _uncomp
// (${address_ipv6_lc_uncomp}, ...) picks between the compressed
// (2001:db8::7) and the fully zero-padded eight-group form
// (2001:0db8:0000:0000:0000:0000:0000:0007). The address is the source
// address of the default route when there is one, otherwise the first
// non-loopback, non-link-local address the host name resolves to; a machine
// with no usable address in a family keeps that family's tokens unresolved.
//
// This is the half of expand_hostname which is safe to apply to a string that
// is not a host name spec - a settings context or an attachment path, say -
// since it only ever replaces a placeholder and never reinterprets the string
// as a whole (see the "auto" shorthands in expand_hostname).
std::string expand_hostname_placeholders(std::string spec);

// The same substitution, but every substituted value is first passed through
// sanitize_path_component. Use this - not the plain variant - whenever the
// result names something on the local file system (an attachment target, a
// settings context): the host name is not fully under the operator's control
// (DHCP can set it on some systems), and a value carrying '/', '\' or a
// dots-only component must not be able to redirect a path the agent reads or
// writes with its (typically root/SYSTEM) privileges.
std::string expand_hostname_placeholders_in_path(std::string spec);

// Format an IPv6 address for the ${address_ipv6*} placeholders: `compressed`
// picks between the RFC 5952 elided form (2001:db8::7) and the fully
// zero-padded eight-group form (2001:0db8:0000:0000:0000:0000:0000:0007),
// `uppercase` the case of the hex digits. Exposed for unit testing - the
// placeholder machinery resolves the address itself.
std::string format_ipv6(const boost::asio::ip::address_v6& address, bool uppercase, bool compressed);

// Reduce `value` to characters safe inside a single path component: letters,
// digits, '.', '_' and '-' pass, anything else becomes '_', and a value that
// is nothing but dots ("." / "..") becomes "_". A legal RFC-952 host name
// comes through unchanged.
std::string sanitize_path_component(std::string value);

// Resolve a hostname spec used by the various submit-clients.
//   "auto"     -> system host name as-is
//   "auto-lc"  -> system host name, lower-cased
//   "auto-uc"  -> system host name, upper-cased
//   anything else: expand_hostname_placeholders above.
std::string expand_hostname(std::string spec);

class socket_exception : public std::exception {
  std::string error;

 public:
  //////////////////////////////////////////////////////////////////////////
  /// Constructor takes an error message.
  /// @param error the error message
  explicit socket_exception(std::string error) noexcept : error(std::move(error)) {}
  socket_exception(const socket_exception& other) noexcept : socket_exception(other.reason()) {}
  ~socket_exception() noexcept override = default;

  //////////////////////////////////////////////////////////////////////////
  /// Retrieve the error message from the exception.
  /// @return the error message
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
// Parse a `tls version` setting into the context method to construct.
// An exact version ("1.2", "tlsv1.3") maps to the version-pinned method, which
// negotiates that version ONLY. A trailing '+' ("1.2+") and the value "any"
// map to the generic TLS method, which negotiates the highest version both
// sides support; the floor a '+' form asks for is NOT part of the method -
// apply it with apply_tls_min_version() on the constructed context, or the
// '+' silently means "any".
boost::asio::ssl::context_base::method tls_method_parser(const std::string& tls_version);
// The minimum protocol version a `tls version` setting asks for: the matching
// SSL TLS1_x_VERSION constant for a '+' form ("1.2+"), 0 when the setting
// carries no floor (exact versions pin via the method; "any" has no floor).
long tls_min_version_parser(const std::string& tls_version);
// Apply the floor a '+' form asks for to a constructed context; no-op for
// settings without one. Every context built from tls_method_parser() needs
// this, or "1.2+" degrades to "any".
void apply_tls_min_version(boost::asio::ssl::context& ctx, const std::string& tls_version);
boost::asio::ssl::verify_mode verify_mode_parser(const std::string& verify_mode);

// Whole days until the peer's certificate expires, negative once it already
// has. Returns none when the peer presented no certificate at all, so a caller
// can tell that apart from "expired a day ago" - collapsing both to -1 loses a
// distinction that matters when the number drives an alert.
boost::optional<long> peer_certificate_expiry_days(SSL* ssl);
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

  boost::optional<boost::system::error_code> write_result;
  // Record both success and failure here (unlike the timer, whose handler
  // ignores its own cancellation). If write errors were dropped, the call would
  // block until the timeout elapsed and then report a (false) timeout instead.
  async_write(sock, buffers, [&write_result](const auto& e) { write_result = e; });

  io_service.restart();
  while (io_service.run_one()) {
    if (write_result) {
      // cancel() can throw (the non-throwing cancel(ec) overload is removed
      // under BOOST_ASIO_NO_DEPRECATED). Swallow that incidental failure so a
      // completed write is reported reliably; a genuine write error is still
      // surfaced explicitly below.
      try {
        timer.cancel();
      } catch (...) {
      }
      if (*write_result) throw boost::system::system_error(*write_result);
      return true;
    } else if (timer_result) {
      rawSocket.close();
      return false;
    }
  }

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
  // Record both success and failure here (unlike the timer, whose handler
  // ignores its own cancellation). If read errors were dropped, the call would
  // block until the timeout elapsed and then report a (false) timeout instead.
  async_read(sock, buffers, [&read_result](const auto& e) { read_result = e; });

  io_service.restart();
  while (io_service.run_one()) {
    if (read_result) {
      // cancel() can throw (the non-throwing cancel(ec) overload is removed
      // under BOOST_ASIO_NO_DEPRECATED). Swallow that incidental failure so a
      // completed read is reported reliably; a genuine read error is still
      // surfaced explicitly below.
      try {
        timer.cancel();
      } catch (...) {
      }
      if (*read_result) throw boost::system::system_error(*read_result);
      return true;
    } else if (timer_result) {
      rawSocket.close();
      return false;
    }
  }

  return false;
}
}  // namespace io
}  // namespace socket_helpers
