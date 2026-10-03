// SPDX-FileCopyrightText: 2013 Grégoire Passault
// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "Controller.h"
#include "dll_defines.hpp"

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose {

class NSCP_MONGOOSE_EXPORT WebLogger {
 public:
  virtual ~WebLogger() = default;
  virtual void log_error(const std::string &message) = 0;
  virtual void log_info(const std::string &message) = 0;
  virtual void log_debug(const std::string &message) = 0;
};

typedef std::shared_ptr<WebLogger> WebLoggerPtr;

class NSCP_MONGOOSE_EXPORT Server {
 public:
  static Server *make_server(const WebLoggerPtr &logger);

  virtual ~Server() = default;

  /**
   * Runs the Mongoose server
   *
   * @return true when the server is listening. A failure (bad bind address,
   *         port in use, TLS setup refused) has already been logged; the
   *         caller only decides what to report about it.
   */
  virtual bool start(const std::string &bind) = 0;

  /**
   * Stops the Mongoose server
   */
  virtual void stop() = 0;

  /**
   * Register a new controller on the server.
   *
   * Thread-safety: always safe to call before `start()`. After `start()` it
   * is backend-specific — the Beast backend snapshots the controller list
   * per-request under a mutex, so concurrent registration is safe; the
   * mongoose backend serves traffic on a single poll thread and races a
   * `vector::push_back` against that thread's read, so with the mongoose
   * backend all controllers must be registered before `start()`. Registering
   * everything before `start()` is the portable pattern.
   *
   * @param controller a pointer to a controller (server takes ownership)
   */
  virtual void registerController(Controller *controller) = 0;

  /**
   * Setup the SSL options.
   *
   * Should be called before `start()`. Behaviour after `start()` is
   * backend-specific: the Beast backend ignores the call and logs a warning
   * (its SSL context is built once at `start()`), while the mongoose backend
   * applies the new cert/key to subsequent connections. Set before `start()`
   * for consistent behaviour across backends.
   *
   * @param certificate path to the PEM-encoded certificate
   * @param key path to the PEM-encoded private key (may equal `certificate`
   *            if the key is concatenated into the cert file)
   * @return true when a certificate and key were loaded. On false the
   *         server is poisoned for TLS: start() refuses to run rather than
   *         fall back to plain HTTP, since both backends decide TLS by whether
   *         a certificate is loaded and a failed load would otherwise leave
   *         the listener in cleartext. Not calling setSsl() at all is how a
   *         caller asks for plain HTTP.
   */
  virtual bool setSsl(std::string &certificate, std::string &key) = 0;

  /**
   * Cap the per-request HTTP body size the server will buffer (bytes).
   *
   * Honored by the Beast backend (default 1 MiB). The mongoose backend
   * uses a compile-time limit and ignores this call. Must be set before
   * `start()` for the change to take effect.
   */
  virtual void setBodyLimit(std::size_t /*bytes*/) {}

  /**
   * How many threads run request handlers. Must be set before `start()`.
   *
   * The default, 1, runs every handler on the server's single I/O thread, so
   * one slow handler holds up every other request and TLS handshake until it
   * returns. A server whose handlers can block for long (running a check, an
   * external script) asks for more. The controllers it registers must then
   * be safe to call concurrently.
   *
   * The Beast backend runs its io_context on this many threads. The mongoose
   * backend keeps its single poll thread for the sockets and hands each
   * request to a pool of this many workers, answering through mg_wakeup().
   */
  virtual void setWorkerThreads(std::size_t /*threads*/) {}

  /**
   * Decide, per connection, whether a peer may connect at all. Called with the
   * peer's address as each connection is accepted - before the TLS handshake
   * and before any request is read - and a peer it refuses is disconnected
   * there. An allow-list belongs here rather than in a controller: checked
   * per request, a refused host can still complete handshakes and occupy the
   * request handlers, and a request no controller routes is answered before
   * the check runs. Must be set before `start()`; called from the server's
   * I/O thread(s), so it must be thread-safe and quick. `remote_ip` is a bare
   * address that boost::asio::ip::make_address() parses - never in brackets
   * (an IPv6 peer may come in the short or the full form depending on the
   * backend).
   *
   * Pure virtual on purpose: an allow-list enforced only here must not be
   * silently ignored by a backend that forgot it.
   */
  typedef std::function<bool(const std::string &remote_ip)> accept_filter;
  virtual void setAcceptFilter(accept_filter filter) = 0;

  /**
   * Whether the calling thread is one of this server's own (an I/O thread or
   * a request worker) - that is, whether the caller is inside a request
   * handler of this server.
   */
  virtual bool isServerThread() const = 0;

  /**
   * Name the server's threads and say where their guard lines go.
   *
   * The threads are started through threads::start_guarded_thread, whose
   * "Thread '<name>': terminated by an uncaught exception" line operators
   * alert on, so the name is what tells two listeners apart ("web server",
   * "ncpa server"; workers get " worker" appended). `reporter` receives that
   * finished line and must not reword it - pass the module's
   * NSC_THREAD_REPORTER. By default the threads are called "web server" and
   * report through the server's WebLogger. Must be set before `start()`.
   */
  typedef std::function<void(const std::string &line)> thread_reporter;
  virtual void setThreadReporting(const std::string & /*thread_name*/, thread_reporter /*reporter*/) {}

  /**
   * Restrict the TLS versions and cipher suites the listener negotiates.
   *
   * `tls_version` uses the same vocabulary as the NRPE and NSCA listeners:
   * an exact version (1.0, 1.1, 1.2, 1.3, optionally spelled tlsv1.2), a
   * trailing `+` for "that version or later", or `any`. `ciphers` is an
   * OpenSSL cipher list, empty meaning the library default.
   *
   * Honoured by the Beast backend, which also applies its own default when
   * handed an empty `tls_version` - so a caller passes empty for a setting the
   * operator never touched. The mongoose backend drives TLS through mongoose's
   * own stack, which does not expose either knob; it logs that a value the
   * operator set is being ignored rather than pretending to apply it, and
   * records the limitation at debug level when nothing was set. Must be called
   * before `start()`.
   */
  virtual void setTlsOptions(const std::string & /*tls_version*/, const std::string & /*ciphers*/) {}
};

/**
 * Stop `server` and free it, from any thread; `server` is empty afterwards.
 *
 * From outside the server this is stop() and then the destructor. From one of
 * the server's own threads - a request handler whose work ended up stopping
 * its own listener - neither can run there: the thread cannot join itself,
 * and the other threads are still inside sessions and handlers that use the
 * server's state (its io_context, acceptor, TLS context, controllers). So the
 * server is handed to a guarded thread of its own, which stops it the normal
 * way - joining every thread, the caller's included once its handler has
 * returned - and only then frees it. `reporter` takes that thread's guard
 * line (pass the module's NSC_THREAD_REPORTER).
 */
NSCP_MONGOOSE_EXPORT void stop_and_release(std::shared_ptr<Server> &server, const Server::thread_reporter &reporter);
}  // namespace Mongoose
