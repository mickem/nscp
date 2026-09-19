// SPDX-FileCopyrightText: 2013 Grégoire Passault
// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
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
   */
  virtual void start(const std::string &bind) = 0;

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
   */
  virtual void setSsl(std::string &certificate, std::string &key) = 0;

  /**
   * Cap the per-request HTTP body size the server will buffer (bytes).
   *
   * Honored by the Beast backend (default 1 MiB). The mongoose backend
   * uses a compile-time limit and ignores this call. Must be set before
   * `start()` for the change to take effect.
   */
  virtual void setBodyLimit(std::size_t /*bytes*/) {}

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
}  // namespace Mongoose
