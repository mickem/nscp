#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/thread/thread.hpp>

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
 * coroutines via `boost::asio::spawn` (decision 1).
 */
namespace Mongoose {

class NSCAPI_EXPORT ServerBeastImpl final : public Server {
 public:
  explicit ServerBeastImpl(WebLoggerPtr logger);
  ~ServerBeastImpl() override;

  void start(const std::string& bind) override;
  void stop() override;
  void registerController(Controller* controller) override;
  void setSsl(std::string& certificate, std::string& key) override;
  void setBodyLimit(std::size_t bytes) override;

  /** Per-connection HTTP body cap. Default 1 MiB. */
  static constexpr std::size_t kDefaultBodyLimit = 1u * 1024u * 1024u;

 private:
  // The accept loop, run as a stackful coroutine off `start()`. Accepts
  // connections until the acceptor is closed (stop()) or a genuine accept
  // error occurs, spawning a per-connection session coroutine for each.
  void accept_loop(const boost::asio::yield_context& yield);

  // Per-connection session coroutines: each owns its socket (moved in),
  // reads one request, dispatches it, writes the response and closes.
  // Exceptions are caught and logged so a single misbehaving client can't
  // tear down the io_context. Split out of the accept loop so each path is
  // readable on its own and can be exercised in isolation.
  void run_tls_session(boost::asio::ip::tcp::socket socket, std::string remote_ip, const boost::asio::yield_context& yield);
  void run_plain_session(boost::asio::ip::tcp::socket socket, std::string remote_ip, const boost::asio::yield_context& yield);

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
  std::size_t body_limit_ = kDefaultBodyLimit;
  // Snapshot-on-read protection for `controllers_`. registerController()
  // may be called from any thread; dispatch coroutines copy the vector
  // briefly under the lock and iterate the snapshot lock-free.
  mutable std::mutex controllers_mu_;
  std::vector<Controller*> controllers_;

  boost::asio::io_context ioc_;
  std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
  std::unique_ptr<boost::asio::ssl::context> ssl_ctx_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
  std::shared_ptr<boost::thread> thread_;
  std::atomic<bool> stopping_{false};
};

}  // namespace Mongoose
