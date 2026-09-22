// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ServerBeastImpl.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <openssl/ssl.h>
#include <boost/thread/thread.hpp>
#include <boost/version.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <threads/guarded_io_context.hpp>
#include <utility>

#include <net/tls_versions.hpp>

#include "Helpers.h"
#include "Request.h"
#include "Response.h"
#include "cert_loader.h"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

namespace {

// boost::asio::spawn's completion-token form — spawn(ex, fn, token) — was
// introduced in Boost 1.80. Older Boost (RHEL/Rocky 9 ships 1.75) only has
// the legacy spawn(ex, fn, attributes) overload, which has no completion
// token, so passing asio::detached there fails to compile. Centralize the
// difference here; every spawn in this file is a fire-and-forget coroutine.
template <typename Fn>
void spawn_detached(asio::io_context& ioc, Fn&& fn) {
#if BOOST_VERSION >= 108000
  asio::spawn(ioc, std::forward<Fn>(fn), asio::detached);
#else
  asio::spawn(ioc, std::forward<Fn>(fn));
#endif
}

struct BindEndpoint {
  bool tls = false;
  std::string host;
  unsigned short port = 0;
};

// Parse the mongoose-style bind URL ("https://0.0.0.0:8443",
// "http://127.0.0.1:8080" or a bare "host:port"). IPv6 bracket syntax
// is not supported — the existing wrapper never emits it. Throws
// nsclient_exception on malformed input (caller catches and logs).
BindEndpoint parse_bind(const std::string& bind) {
  BindEndpoint ep;
  std::string s = bind;
  if (boost::algorithm::starts_with(s, "https://")) {
    ep.tls = true;
    s.erase(0, 8);
  } else if (boost::algorithm::starts_with(s, "http://")) {
    s.erase(0, 7);
  } else if (boost::algorithm::starts_with(s, "tcp://")) {
    s.erase(0, 6);
  }
  const auto slash = s.find('/');
  if (slash != std::string::npos) s.erase(slash);
  const auto colon = s.rfind(':');
  if (colon == std::string::npos) {
    ep.host = s;
    ep.port = ep.tls ? 443 : 80;
  } else {
    ep.host = s.substr(0, colon);
    unsigned long port_value = 0;
    try {
      std::size_t consumed = 0;
      port_value = std::stoul(s.substr(colon + 1), &consumed);
      // Reject "8443abc" — stoul stops at the first non-digit but we
      // require the whole port substring to be numeric.
      if (consumed != s.size() - colon - 1) {
        throw std::invalid_argument("trailing garbage in port");
      }
    } catch (const std::exception& e) {
      throw nsclient::nsclient_exception("Invalid port in bind '" + bind + "': " + e.what());
    }
    // Catches both >65535 and the silent narrowing-truncation that the
    // old `static_cast<unsigned short>(stoul(...))` would have done.
    if (port_value > 65535U) {
      throw nsclient::nsclient_exception("Port " + std::to_string(port_value) + " out of range (0..65535) in bind '" + bind + "'");
    }
    ep.port = static_cast<unsigned short>(port_value);
  }
  if (ep.host.empty()) ep.host = "0.0.0.0";
  return ep;
}

// Build a Mongoose::Request from a Beast HTTP request. Method overrides
// (X-HTTP-Method-Override) and URL/query split match the mongoose
// implementation in ServerMongooseImpl.cpp:onHttpRequest so controllers
// see identical input on either backend.
Mongoose::Request beast_to_request(const http::request<http::string_body>& req, const std::string& remote_ip, const bool is_ssl) {
  std::string target(req.target().begin(), req.target().end());
  std::string url = target;
  std::string query;
  const auto qpos = target.find('?');
  if (qpos != std::string::npos) {
    url = target.substr(0, qpos);
    query = target.substr(qpos + 1);
  }

  std::string method(req.method_string());

  Mongoose::Request::headers_type headers;
  for (const auto& field : req) {
    std::string name(field.name_string());
    std::string value(field.value());
    headers[name] = value;
  }

  // Case-insensitive full-name match, matching ServerMongooseImpl. headers_type
  // is an ordinary case-sensitive map (see L13 in the security review for the
  // general problem), so this scans rather than relying on find().
  //
  // Scanned over `req` rather than `headers`: the map above collapses repeated
  // fields to the last value and folds nothing by case, so it cannot tell a
  // single override from several. A repeated override is ambiguous and is
  // dropped, not resolved - if this backend picked "first wins" while the
  // mongoose backend picked "last wins", a proxy ACL that classifies by method
  // could be bypassed by sending both.
  std::string override_value;
  std::size_t override_count = 0;
  for (const auto& field : req) {
    if (!boost::algorithm::iequals(std::string(field.name_string()), "X-HTTP-Method-Override")) continue;
    std::string value(field.value());
    if (value.empty()) continue;
    override_value = std::move(value);
    override_count++;
  }
  if (override_count == 1) {
    method = override_value;
  }

  return {remote_ip, is_ssl, std::move(method), std::move(url), std::move(query), std::move(headers), req.body()};
}

// Translate a Mongoose::Response into a Beast response. Mirrors the
// header / cookie / Content-Type defaults emitted by
// ServerMongooseImpl::onHttpRequest, including the SameSite=None +
// Secure RFC 6265bis guard (covered by the existing Server_test cases).
void mongoose_to_beast(Mongoose::Response& src, http::response<http::string_body>& dst, const bool is_ssl) {
  dst.result(static_cast<unsigned>(src.getCode()));
  dst.body() = src.getBody();

  bool has_content_type = false;
  for (const auto& kv : src.get_headers()) {
    // Defense against HTTP response splitting: a controller that ever
    // reflects user input into a header name or value would otherwise
    // let an attacker inject CR/LF and forge a second header (or even a
    // second response). Drop the entire header pair if either side
    // contains CR or LF — mongoose's wrapper has the same gap, this
    // closes it in the new backend.
    if (kv.first.find_first_of("\r\n") != std::string::npos) continue;
    if (kv.second.find_first_of("\r\n") != std::string::npos) continue;
    dst.insert(kv.first, kv.second);
    if (kv.first == "Content-Type") has_content_type = true;
  }

  for (const auto& entry : src.get_cookies()) {
    const std::string& name = entry.first;
    const std::string& value = entry.second.first;
    const Mongoose::Response::cookie_attrs& a = entry.second.second;
    if (name.empty() || name.find_first_of("\r\n;= \t") != std::string::npos) continue;
    if (value.find_first_of("\r\n;") != std::string::npos) continue;
    // path and same_site are emitted into the Set-Cookie header verbatim, and
    // Response::setCookie() stores them unsanitized — so a controller could
    // smuggle CR/LF (response splitting) or ';' (forged extra attributes)
    // through them. Drop the whole cookie if either is tainted.
    if (a.path.find_first_of("\r\n;") != std::string::npos) continue;
    if (a.same_site.find_first_of("\r\n;") != std::string::npos) continue;
    if (boost::algorithm::iequals(a.same_site, "None") && !(a.secure && is_ssl)) continue;

    std::string cookie = name + "=" + value + "; Path=" + (a.path.empty() ? "/" : a.path);
    if (a.max_age >= 0) cookie += "; Max-Age=" + std::to_string(a.max_age);
    if (a.http_only) cookie += "; HttpOnly";
    if (a.secure && is_ssl) cookie += "; Secure";
    if (!a.same_site.empty()) cookie += "; SameSite=" + a.same_site;
    // `insert` (not `set`) so multiple Set-Cookie headers are preserved
    // verbatim — matches mongoose, which emits each one as its own line.
    dst.insert(http::field::set_cookie, cookie);
  }

  if (src.getCode() == 200 && !has_content_type) dst.set(http::field::content_type, "application/json");
  if (src.getCode() > 299 && !has_content_type) dst.set(http::field::content_type, "text/plain");
  dst.prepare_payload();
}

}  // namespace

namespace Mongoose {

ServerBeastImpl::ServerBeastImpl(WebLoggerPtr logger) : logger_(std::move(logger)) {
  // Keep the io_context alive after start() spawns the accept coroutine
  // but before the first work has been queued onto it. Released in stop().
  work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(ioc_.get_executor());
}

ServerBeastImpl::~ServerBeastImpl() {
  ServerBeastImpl::stop();
  for (const Controller* c : controllers_) delete c;
  controllers_.clear();
}

void ServerBeastImpl::setSsl(std::string& certificate, std::string& key) {
  if (thread_) {
    // ssl_ctx_ is built once in start() from the then-current PEM
    // strings. Mutating cert_pem_/key_pem_ now wouldn't reach the live
    // SSL context. Refuse to silently mislead the caller.
    logger_->log_error("setSsl() called after start() — ignored; restart the server to apply a new certificate");
    return;
  }
  try {
    auto cert_and_key = cert_loader::load_certificates(certificate, key);
    cert_pem_ = std::move(cert_and_key.first);
    key_pem_ = std::move(cert_and_key.second);
  } catch (const nsclient::nsclient_exception& e) {
    logger_->log_error("Failed to load certificates: " + e.reason());
  }
}

void ServerBeastImpl::registerController(Controller* controller) {
  std::lock_guard<std::mutex> g(controllers_mu_);
  controllers_.push_back(controller);
}

void ServerBeastImpl::setBodyLimit(std::size_t bytes) {
  if (thread_) {
    logger_->log_error("setBodyLimit() called after start() — ignored; restart the server to apply");
    return;
  }
  body_limit_ = bytes;
}

void ServerBeastImpl::setTlsOptions(const std::string& tls_version, const std::string& ciphers) {
  if (thread_) {
    logger_->log_error("setTlsOptions() called after start() — ignored; restart the server to apply");
    return;
  }
  if (!tls_version.empty()) tls_version_ = tls_version;
  ciphers_ = ciphers;
}

namespace {
// Resolve a `tls version` spec into an OpenSSL min/max pair, from the same
// table the NRPE and NSCA listeners read (<net/tls_versions.hpp>), so one
// setting means the same thing everywhere in the agent: an exact version, a
// trailing `+` for "that version or later", or `any`. Returns false with
// `error` filled in for a spelling this listener cannot honour, so it refuses
// to start rather than quietly serving whatever the library defaults to.
bool resolve_tls_range(const std::string& spec, long& min_version, long& max_version, std::string& error) {
  std::string lower = boost::algorithm::to_lower_copy(spec);
  boost::algorithm::trim(lower);
  if (lower.empty() || lower == "any") {
    min_version = 0;
    max_version = TLS1_3_VERSION;
    return true;
  }
  const bool open_ended = lower.back() == '+';
  if (open_ended) lower.pop_back();
  if (!tls_versions::lookup(lower, min_version)) {
    error = "Invalid tls version: " + spec;
    return false;
  }
  if (!open_ended && min_version == SSL3_VERSION) {
    // The context below excludes SSL 3.0 unconditionally, and rightly so:
    // POODLE, and most OpenSSL builds no longer compile it in. Pinning both
    // ends of the range to it would therefore start a listener that reports
    // success and then fails every handshake, with nothing logged. Say so
    // instead. (`sslv3+` is a floor, not a pin, and stays accepted - it is
    // raised below.)
    error = "Invalid tls version: " + spec + " (SSL 3.0 is never served; use 1.0+ for the widest range this listener accepts)";
    return false;
  }
  // A floor of SSL 3.0 means "oldest we speak" rather than SSL 3.0 itself, and
  // SSL_CTX_set_min_proto_version(SSL3_VERSION) fails outright on a build
  // without SSL 3.0. Raise it to the oldest version that can actually be
  // negotiated; no_sslv3 already decides the rest.
  if (min_version == SSL3_VERSION) min_version = TLS1_VERSION;
  max_version = open_ended ? TLS1_3_VERSION : min_version;
  return true;
}
}  // namespace

void ServerBeastImpl::dispatch(const http::request<http::string_body>& req, http::response<http::string_body>& res, const std::string& remote_ip, const bool is_ssl) {
  // Snapshot the controller list under the lock. registerController()
  // may be racing with us — by copying the pointers out we let the
  // ::handles / ::handleRequest dispatch run lock-free, which matters
  // because handleRequest can call into user code that takes its own
  // time and we don't want to block registrations on that.
  std::vector<Controller*> snapshot;
  {
    std::lock_guard<std::mutex> g(controllers_mu_);
    snapshot = controllers_;
  }

  Request request = beast_to_request(req, remote_ip, is_ssl);
  Response* matched = nullptr;
  for (Controller* ctrl : snapshot) {
    if (ctrl->handles(request.getMethod(), request.getUrl())) {
      matched = ctrl->handleRequest(request);
      break;
    }
  }
  std::unique_ptr<Response> owner(matched);
  if (matched) {
    // Applied here, on the way out, so every answer carries them: static
    // files, API responses and the error pages a controller returns alike.
    Helpers::add_security_headers(*matched, is_ssl);
    mongoose_to_beast(*matched, res, is_ssl);
  } else {
    res.result(http::status::not_found);
    res.body() = "Document not found";
    res.set(http::field::content_type, "text/plain");
    // The 404 for an unmatched URL never goes through a Response, so it needs
    // the same treatment directly. A framed 404 is still a framed page. From
    // the shared list rather than written out again: this used to hand-roll a
    // third, narrower policy, and a test asserting only frame-ancestors kept
    // the divergence invisible.
    for (const auto& header : Helpers::security_headers(is_ssl)) {
      res.set(header.first, header.second);
    }
    res.prepare_payload();
  }
}

void ServerBeastImpl::start(const std::string& bind) {
  if (thread_) {
    // Already running — refuse a concurrent double-start. (A start() after a
    // matching stop() is fine and supported: stop() clears thread_, and the
    // run-state reset below re-arms the io_context so the instance can be
    // unloaded/reloaded.)
    logger_->log_error("start() called on an already-started server — ignored");
    return;
  }

  BindEndpoint ep;
  try {
    ep = parse_bind(bind);
  } catch (const nsclient::nsclient_exception& e) {
    logger_->log_error(e.reason());
    return;
  }

  // TLS is driven by "did setSsl() load a cert" — matches the mongoose
  // backend's behaviour (ServerMongooseImpl::initTls gates on cert
  // presence, not the URL scheme). WEBServer always calls start() with
  // a scheme-less "0.0.0.0:<port>", so keying off the scheme alone
  // would silently leave the server in plain-HTTP mode and curl's TLS
  // ClientHello would be parsed as garbage HTTP and reset the
  // connection. The `https://` scheme is still accepted as an explicit
  // hint when present.
  use_tls_ = ep.tls || (!cert_pem_.empty() && !key_pem_.empty());

  if (use_tls_) {
    if (cert_pem_.empty() || key_pem_.empty()) {
      logger_->log_error("TLS requested for " + bind + " but no certificate/key was set");
      return;
    }
    // asio's tlsv12_server pins *both* ends of the range to TLS 1.2, so this
    // listener could never negotiate TLS 1.3 - on every DEB/RPM build, where
    // beast is the backend. The generic tls_server method plus an explicit
    // floor is what the rest of the agent does, and it makes `tls version`
    // and `allowed ciphers` mean the same here as on the NRPE and NSCA
    // listeners.
    long min_version = 0;
    long max_version = 0;
    std::string version_error;
    if (!resolve_tls_range(tls_version_, min_version, max_version, version_error)) {
      logger_->log_error(version_error + ". The WEB server has NOT been started.");
      return;
    }
    ssl_ctx_ = std::make_unique<asio::ssl::context>(asio::ssl::context::tls_server);
    ssl_ctx_->set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 | asio::ssl::context::single_dh_use);
    if (SSL_CTX_set_min_proto_version(ssl_ctx_->native_handle(), min_version) != 1 ||
        SSL_CTX_set_max_proto_version(ssl_ctx_->native_handle(), max_version) != 1) {
      logger_->log_error("Failed to apply tls version '" + tls_version_ + "': rejected by this OpenSSL build. The WEB server has NOT been started.");
      ssl_ctx_.reset();
      return;
    }
    if (!ciphers_.empty() && SSL_CTX_set_cipher_list(ssl_ctx_->native_handle(), ciphers_.c_str()) != 1) {
      // Refuse rather than fall back: a cipher list that OpenSSL rejects
      // leaves the default suite set in place, which is the opposite of what
      // an operator narrowing it asked for.
      logger_->log_error("Failed to apply the configured allowed ciphers: rejected by this OpenSSL build. The WEB server has NOT been started.");
      ssl_ctx_.reset();
      return;
    }
    try {
      ssl_ctx_->use_certificate_chain(asio::buffer(cert_pem_));
      ssl_ctx_->use_private_key(asio::buffer(key_pem_), asio::ssl::context::pem);
    } catch (const std::exception& e) {
      logger_->log_error(std::string("Failed to install certificate: ") + e.what());
      ssl_ctx_.reset();
      return;
    }
  }

  boost::system::error_code ec;
  const auto address = asio::ip::make_address(ep.host, ec);
  if (ec) {
    logger_->log_error("Invalid bind address '" + ep.host + "': " + ec.message());
    return;
  }
  const tcp::endpoint endpoint(address, ep.port);

  acceptor_ = std::make_unique<tcp::acceptor>(ioc_);
  acceptor_->open(endpoint.protocol(), ec);
  if (ec) {
    logger_->log_error("Failed to open acceptor: " + ec.message());
    acceptor_.reset();
    return;
  }
  acceptor_->set_option(asio::socket_base::reuse_address(true), ec);
  acceptor_->bind(endpoint, ec);
  if (ec) {
    logger_->log_error("Failed to bind " + bind + ": " + ec.message());
    acceptor_.reset();
    return;
  }
  acceptor_->listen(asio::socket_base::max_listen_connections, ec);
  if (ec) {
    logger_->log_error("Failed to listen on " + bind + ": " + ec.message());
    acceptor_.reset();
    return;
  }

  // (Re)initialize the per-run io_context state so the instance can be
  // restarted after a stop() (e.g. plugin unload/reload). A previous run
  // left ioc_ run-to-completion, the work guard released, and stopping_ set;
  // undo all three before spawning the new accept loop. Safe here because the
  // guard above guarantees no run() is in flight (thread_ is null/joined).
  stopping_ = false;
  ioc_.restart();
  work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(ioc_.get_executor());

  // Accept loop runs in its own coroutine so a slow handshake/read on one
  // connection can't block the accept side (each connection gets its own
  // session coroutine — see accept_loop / run_*_session).
  spawn_detached(ioc_, [this](const asio::yield_context& yield) { accept_loop(yield); });

  // A handler that throws used to end the web server for the lifetime of the
  // process - the exception was logged, but the one thread running the event
  // loop was gone and nothing restarted it. Re-enter instead; the request
  // fails, the server keeps serving.
  const WebLoggerPtr log = logger_;
  thread_ = threads::start_guarded_thread(
      "web server",
      [this, log] {
        threads::run_io_context_guarded("web server", ioc_,
                                        [log](const std::string& name, const std::string& detail) { log->log_error("Thread '" + name + "': " + detail); });
      },
      [log](const std::string& name, const std::string& detail) { log->log_error("Thread '" + name + "': " + detail); });
}

void ServerBeastImpl::accept_loop(const asio::yield_context& yield) {
  while (!stopping_) {
    boost::system::error_code aec;
    tcp::socket socket(ioc_);
    acceptor_->async_accept(socket, yield[aec]);
    if (aec) {
      // operation_aborted is the normal stop() path (the acceptor was
      // closed); anything else is a genuine failure — log it before we
      // stop accepting so the silence isn't mistaken for a clean shutdown.
      if (aec != boost::asio::error::operation_aborted) {
        logger_->log_error("Accept failed, stopping accept loop: " + aec.message());
      }
      return;
    }

    std::string remote;
    boost::system::error_code rec;
    const auto re = socket.remote_endpoint(rec);
    if (!rec) remote = re.address().to_string();

    // Each connection is handled in its own coroutine so one slow client
    // never holds up the accept side.
    if (use_tls_) {
      spawn_detached(ioc_, [this, sock = std::move(socket), remote](const asio::yield_context& y) mutable { run_tls_session(std::move(sock), remote, y); });
    } else {
      spawn_detached(ioc_, [this, sock = std::move(socket), remote](const asio::yield_context& y) mutable { run_plain_session(std::move(sock), remote, y); });
    }
  }
}

void ServerBeastImpl::run_tls_session(tcp::socket socket, std::string remote_ip, const asio::yield_context& yield) {
  try {
    asio::ssl::stream<beast::tcp_stream> stream(beast::tcp_stream(std::move(socket)), *ssl_ctx_);
    boost::system::error_code sec;
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
    stream.async_handshake(asio::ssl::stream_base::server, yield[sec]);
    if (sec) return;

    beast::flat_buffer buffer;
    // Explicit parser so the per-request body cap is visible and tunable
    // (Beast's default body_limit is 1 MiB; we make it explicit +
    // configurable via setBodyLimit()).
    http::request_parser<http::string_body> parser;
    parser.body_limit(body_limit_);
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
    http::async_read(stream, buffer, parser, yield[sec]);
    if (sec) return;
    http::request<http::string_body> req = parser.release();

    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(false);
    dispatch(req, res, remote_ip, /*is_ssl=*/true);

    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
    http::async_write(stream, res, yield[sec]);
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(5));
    stream.async_shutdown(yield[sec]);  // graceful TLS close (ignore errors)
  } catch (const std::exception& e) {
    logger_->log_error(std::string("TLS session error: ") + e.what());
  }
}

void ServerBeastImpl::run_plain_session(tcp::socket socket, std::string remote_ip, const asio::yield_context& yield) {
  try {
    beast::tcp_stream stream(std::move(socket));
    boost::system::error_code sec;
    beast::flat_buffer buffer;
    http::request_parser<http::string_body> parser;
    parser.body_limit(body_limit_);
    stream.expires_after(std::chrono::seconds(30));
    http::async_read(stream, buffer, parser, yield[sec]);
    if (sec) return;
    http::request<http::string_body> req = parser.release();

    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(false);
    dispatch(req, res, remote_ip, /*is_ssl=*/false);

    stream.expires_after(std::chrono::seconds(30));
    http::async_write(stream, res, yield[sec]);
    boost::system::error_code shutdown_ec;
    stream.socket().shutdown(tcp::socket::shutdown_send, shutdown_ec);
  } catch (const std::exception& e) {
    logger_->log_error(std::string("Session error: ") + e.what());
  }
}

void ServerBeastImpl::stop() {
  if (!thread_) return;
  stopping_ = true;

  // Drain naturally — DO NOT call `ioc_.stop()`. Calling stop() while a
  // `boost::asio::spawn` coroutine is suspended leaves the coroutine's
  // handler chain queued in the scheduler; `~io_context()` then walks
  // that queue and the spawn machinery dereferences a stale
  // `spawned_thread_base*`, which ASan flags as SEGV at
  // boost/asio/impl/spawn.hpp:385. The supported shutdown is:
  //
  //   1. Close the acceptor → in-flight async_accept errors with
  //      operation_aborted → the accept coroutine returns.
  //   2. Drop the work guard → the io_context loses its artificial
  //      keep-alive.
  //   3. Active per-connection coroutines complete on their own (read
  //      EOF when the client closes, write completion, or the 30s
  //      expires_after deadline at the worst).
  //   4. ioc_.run() exits naturally with no pending operations, so
  //      ~io_context() has nothing left to destroy.
  if (acceptor_ && acceptor_->is_open()) {
    boost::system::error_code ec;
    acceptor_->cancel(ec);
    acceptor_->close(ec);
  }
  if (work_guard_) work_guard_->reset();

  // Stopping the server from the thread that runs it: a request handler took a
  // route that unloads the module (the core refuses that, this is the backstop
  // for any other path). Joining here would join this thread with itself,
  // which throws rather than returning, and the throw would escape a
  // destructor further up. Let it go instead: the acceptor is closed and the
  // work guard dropped, so run() returns as soon as this handler does.
  if (thread_->get_id() == boost::this_thread::get_id()) {
    thread_->detach();
    thread_.reset();
    return;
  }

  thread_->join();
  thread_.reset();
  acceptor_.reset();
  ssl_ctx_.reset();
}

}  // namespace Mongoose
