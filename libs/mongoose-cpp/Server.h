#pragma once

#include <boost/shared_ptr.hpp>
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

typedef boost::shared_ptr<WebLogger> WebLoggerPtr;

class NSCAPI_EXPORT Server {
 public:
  static Server *make_server(WebLoggerPtr logger);

  virtual ~Server() = default;

  /**
   * Runs the Mongoose server
   */
  virtual void start(std::string &bind) = 0;

  /**
   * Stops the Mongoose server
   */
  virtual void stop() = 0;

  /**
   * Register a new controller on the server
   *
   * @param controller a pointer to a controller
   */
  virtual void registerController(Controller *controller) = 0;

  /**
   * Setup the mongoose ssl options section
   *
   * @param certificate the name of the certificate to use
   * @param key the name of the key to use
   */
  virtual void setSsl(std::string &certificate, std::string &key) = 0;

  /**
   * Does the server handles url?
   */
};
}  // namespace Mongoose
