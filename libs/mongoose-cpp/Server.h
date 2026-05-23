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
   * Thread-safety contract: implementations MUST allow this to be called
   * from any thread, both before and after `start()`. The Beast backend
   * snapshots the controller list per-request under a mutex; the mongoose
   * backend serves traffic on a single poll thread so calls from the
   * caller's thread race vector::push_back against the poll thread's
   * read — historically tolerated because callers register all
   * controllers before `start()`, but the safer pattern is still to
   * complete registration before starting.
   *
   * @param controller a pointer to a controller (server takes ownership)
   */
  virtual void registerController(Controller *controller) = 0;

  /**
   * Setup the SSL options.
   *
   * Must be called before `start()`. Calling after `start()` is a no-op
   * (with a logged warning); the live SSL context is only built when
   * `start()` runs.
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
