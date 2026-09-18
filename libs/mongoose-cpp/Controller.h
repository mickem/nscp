// SPDX-FileCopyrightText: 2013 Grégoire Passault
// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <string>

#include "Request.h"
#include "Response.h"
#include "dll_defines.hpp"

/**
 * A controller is a module that respond to requests
 *
 * You can override the preProcess, process and postProcess to answer to
 * the requests
 */
namespace Mongoose {
class NSCP_MONGOOSE_EXPORT Controller {
 public:
  virtual ~Controller() = default;
  /**
   * Handle a request, this will try to match the request, if this
   * controller handles it, it will preProcess, process then postProcess it
   *
   * @param request the request
   *
   * @return Response the created response, or NULL if the controller
   *         does not handle this request
   */
  virtual Response* handleRequest(Request& request) = 0;
  virtual bool handles(std::string method, std::string url) = 0;

  /**
   * Called when an exception occur during the rendering
   *
   * @param message the error message
   *
   * @return response a response to send, 404 will occur if NULL
   */
  static Response* serverInternalError(const std::string& message);
  static Response* documentMissing(const std::string& message);

  /**
   * Sink for text that must reach the agent log but never a client. Installed
   * once by the server; unset it is a no-op.
   */
  typedef std::function<void(const std::string&)> error_sink;
  static void setErrorSink(error_sink sink);

  /**
   * Answer an unhandled handler exception.
   *
   * The detail goes to the sink above and a generic body to the caller. It
   * used to be returned verbatim - make_address on an unavailable peer
   * address, bad_optional_access, a regex_error - to a client that need not
   * have authenticated, which is free reconnaissance of the agent's internals.
   */
  static Response* internalErrorFromException(const std::string& detail);
};
}  // namespace Mongoose
