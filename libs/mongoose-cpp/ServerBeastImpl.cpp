#include "ServerBeastImpl.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/version.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

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

  const auto override = headers.find("X-HTTP-Method-Override");
  if (override != headers.end() && !override->second.empty()) {
    method = override->second;
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
    mongoose_to_beast(*matched, res, is_ssl);
  } else {
    res.result(http::status::not_found);
    res.body() = "Document not found";
    res.set(http::field::content_type, "text/plain");
    res.prepare_payload();
  }
}

void ServerBeastImpl::start(const std::string& bind) {
  if (thread_) {
    // Second start() on the same instance: ioc_ was already stop()'d
    // and the work guard reset, so the new accept coroutine would never
    // run. Refuse loudly rather than appear to start.
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
    ssl_ctx_ = std::make_unique<asio::ssl::context>(asio::ssl::context::tlsv12_server);
    ssl_ctx_->set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 | asio::ssl::context::single_dh_use);
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

  // Accept loop runs in its own coroutine so a slow handshake/read on one
  // connection can't block the accept side (each connection gets its own
  // session coroutine — see accept_loop / run_*_session).
  spawn_detached(ioc_, [this](const asio::yield_context& yield) { accept_loop(yield); });

  thread_ = std::make_shared<boost::thread>([this] {
    try {
      ioc_.run();
    } catch (const std::exception& e) {
      logger_->log_error(std::string("io_context error: ") + e.what());
    }
  });
}

void ServerBeastImpl::accept_loop(const asio::yield_context& yield) {
  while (!stopping_) {
    boost::system::error_code aec;
    tcp::socket socket(ioc_);
    acceptor_->async_accept(socket, yield[aec]);
    if (aec) return;  // operation_aborted on close, or genuine failure

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

  thread_->join();
  thread_.reset();
  acceptor_.reset();
  ssl_ctx_.reset();
}

}  // namespace Mongoose
