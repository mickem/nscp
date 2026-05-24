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

class NSCAPI_EXPORT WebLogger {
 public:
  virtual ~WebLogger() = default;
  virtual void log_error(const std::string &message) = 0;
  virtual void log_info(const std::string &message) = 0;
  virtual void log_debug(const std::string &message) = 0;
};

typedef std::shared_ptr<WebLogger> WebLoggerPtr;

class NSCAPI_EXPORT Server {
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
};
}  // namespace Mongoose
