// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/thread/thread.hpp>
#include <net/connection_log_throttle.hpp>

#include "Controller.h"
#include "Server.h"
#include "dll_defines.hpp"

/**
 * Boost.Beast implementation of `Mongoose::Server`. Each instance is
 * fully self-contained: it owns its own `io_context`, `ssl::context`,
 * `tcp::acceptor` and an I/O thread, so multiple `ServerBeastImpl`
 * objects can run concurrently in the same process with no shared
 * state (matches decision 3 in docs/design/beast-web-backend.md).
 *
 * The accept loop and each per-connection handler run as stackful
 * coroutines via `boost::asio::spawn` (decision 1). A connection is served
 * for as long as the client keeps it alive, one request at a time.
 */
namespace Mongoose {

class NSCP_MONGOOSE_EXPORT ServerBeastImpl final : public Server {
 public:
  explicit ServerBeastImpl(WebLoggerPtr logger);
  ~ServerBeastImpl() override;

  bool start(const std::string& bind) override;
  void stop() override;
  void registerController(Controller* controller) override;
  bool setSsl(std::string& certificate, std::string& key) override;
  void setBodyLimit(std::size_t bytes) override;
  void setWorkerThreads(std::size_t threads) override;
  void setAcceptFilter(accept_filter filter) override;
  bool isServerThread() const override;
  void setThreadReporting(const std::string& thread_name, thread_reporter reporter) override;
  void setTlsOptions(const std::string& tls_version, const std::string& ciphers) override;

  // How long a connection may sit waiting on its client - for the handshake,
  // for the next request on a kept-alive connection - and how long one write
  // may take, before the server gives up on it.
  static constexpr std::chrono::seconds kClientTimeout{30};

  // One connection's stream and the one bit stop() needs to know about it.
  // Shared between the session coroutine and the closer stop() posts to the
  // connection's strand, which is why it is a shared_ptr rather than a stack
  // local of the coroutine.
  template <class Stream>
  struct connection_state {
    template <class... Args>
    explicit connection_state(Args&&... args) : stream(std::forward<Args>(args)...) {}
    Stream stream;
    // True while the coroutine is suspended waiting on the client. Only
    // touched on the connection's strand: the session sets it around its
    // waits and the closer reads it there, so no lock.
    bool idle = false;
  };

 private:
  // The accept loop, run as a stackful coroutine off `start()`. Accepts
  // connections until the acceptor is closed (stop()) or a genuine accept
  // error occurs, spawning a per-connection session coroutine for each.
  void accept_loop(const boost::asio::yield_context& yield);

  // Spawn accept_loop() with a guard that respawns it if it throws.
  //
  // threads::run_io_context_guarded() re-enters run() after a handler throws,
  // which keeps the queued handlers going - but the accept loop is a
  // coroutine, and a coroutine that throws is destroyed. Re-entering run()
  // would then block on the work guard with no acceptor: a web server that is
  // up, logged a recovery and answers nothing. Respawning is what actually
  // recovers it.
  void spawn_accept_loop();

  // Consecutive respawns of the accept loop. An accept loop that throws every
  // time it is spawned would otherwise spin, so it is given a few tries and
  // then left down with a clear message rather than burning a core in
  // silence. Only touched from the io_context thread.
  unsigned int accept_loop_restarts_ = 0;
  static constexpr unsigned int kMaxAcceptLoopRestarts = 5;

  // Per-connection session coroutines: each owns its socket (moved in), sets
  // up the transport (the TLS handshake, or nothing) and hands the stream to
  // serve_connection(). Exceptions are caught and logged so a single
  // misbehaving client can't tear down the io_context. Split out of the
  // accept loop so each path is readable on its own and can be exercised in
  // isolation.
  void run_tls_session(boost::asio::ip::tcp::socket socket, std::string remote_ip, const boost::asio::yield_context& yield);
  void run_plain_session(boost::asio::ip::tcp::socket socket, std::string remote_ip, const boost::asio::yield_context& yield);

  // Serve requests on one connection, back to back, until the client asks
  // for the connection to close (HTTP/1.0, or `Connection: close`), goes
  // away, sits idle for kClientTimeout, sends a body over the cap, or the
  // server stops. The transport is already set up; the stream is what
  // async_read / async_write go through.
  template <class Stream>
  void serve_connection(const std::shared_ptr<connection_state<Stream>>& state, const std::string& remote_ip, bool is_ssl,
                        const boost::asio::yield_context& yield);

  // Answer a request whose body is over the cap with a 413 and close. `parser`
  // holds what was read before the parser refused: the header, which is all
  // the answer needs.
  template <class Stream>
  void reject_oversized(Stream& stream, const boost::beast::http::request_parser<boost::beast::http::string_body>& parser, const std::string& remote_ip,
                        bool is_ssl, const boost::asio::yield_context& yield);

  // Connections that are up, by id, each with a closer that stop() calls.
  // A kept-alive connection waits up to kClientTimeout for its next request;
  // without this, stop() would wait that long too, for every idle browser tab.
  // The closer posts onto the connection's strand and closes the socket only
  // if the session is waiting on the client (connection_state::idle): one that
  // is in a handler or writing an answer finishes that first and finds
  // stopping_ set when it comes back for the next request.
  std::uint64_t track_connection(std::function<void()> closer);
  void forget_connection(std::uint64_t id);
  void close_idle_connections();

  // A connection's registration for its lifetime: tracked on construction,
  // forgotten on destruction, so every return path of a session forgets it.
  class connection_registration {
   public:
    connection_registration(ServerBeastImpl& server, std::function<void()> closer) : server_(server), id_(server.track_connection(std::move(closer))) {}
    ~connection_registration() { server_.forget_connection(id_); }
    connection_registration(const connection_registration&) = delete;
    connection_registration& operator=(const connection_registration&) = delete;

   private:
    ServerBeastImpl& server_;
    std::uint64_t id_;
  };

  // The body cap is applied before authentication, so any host that can reach
  // the port can have a line logged per connection; this lets one through per
  // address per minute, with the count of the rest.
  net::connection_log_throttle oversized_log_;

  // Snapshot controllers_ under the mutex, run the matching one (if any),
  // and translate the result into `res`. Falls back to a plain 404 when
  // no controller matches. Kept out-of-line so both the TLS and plain
  // dispatch paths share the same logic.
  void dispatch(const boost::beast::http::request<boost::beast::http::string_body>& req,
                boost::beast::http::response<boost::beast::http::string_body>& res,
                const std::string& remote_ip,
                bool is_ssl);

  WebLoggerPtr logger_;
  std::string cert_pem_;
  std::string key_pem_;
  bool use_tls_ = false;
  // setSsl() was called and the certificate did not load: start() refuses.
  bool ssl_failed_ = false;
  // The TLS version range and cipher list the operator configured. Defaults
  // to "1.2+" so the listener keeps accepting TLS 1.2 while now also being
  // able to negotiate TLS 1.3, which the hard-coded tlsv12_server method made
  // impossible on every Linux build.
  std::string tls_version_ = "1.2+";
  std::string ciphers_;
  std::size_t body_limit_ = Server::kDefaultBodyLimit;
  // Snapshot-on-read protection for `controllers_`. registerController()
  // may be called from any thread; dispatch coroutines copy the vector
  // briefly under the lock and iterate the snapshot lock-free.
  mutable std::mutex controllers_mu_;
  std::vector<Controller*> controllers_;

  boost::asio::io_context ioc_;
  std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
  std::unique_ptr<boost::asio::ssl::context> ssl_ctx_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
  // The threads running ioc_; empty while stopped. More than one when
  // setWorkerThreads() asked for it, so a handler that blocks holds up only
  // the thread it runs on.
  std::vector<std::shared_ptr<boost::thread>> threads_;
  // The ids of threads_, for isServerThread(): asked from a request handler
  // on one of those threads while stop() on another may be joining, detaching
  // and clearing threads_ itself, so it reads a copy under its own lock. Kept
  // in step with threads_ where that changes. Never held across a join.
  mutable std::mutex thread_ids_mu_;
  std::set<boost::thread::id> thread_ids_;
  std::size_t worker_threads_ = 1;
  accept_filter accept_filter_;
  std::string thread_name_ = "web server";
  thread_reporter thread_reporter_;
  std::atomic<bool> stopping_{false};
  // See track_connection(). Touched from the session coroutines (any io
  // thread) and from stop() (any thread), hence the lock; never held across a
  // join or a post that could block.
  mutable std::mutex connections_mu_;
  std::map<std::uint64_t, std::function<void()>> connections_;
  std::uint64_t next_connection_id_ = 0;
};

}  // namespace Mongoose
