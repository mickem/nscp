// SPDX-FileCopyrightText: 2013 Grégoire Passault
// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: MIT

#ifndef _MONGOOSE_REQUEST_HANDLER_H
#define _MONGOOSE_REQUEST_HANDLER_H

#include <memory>
#include <string>

#include "Controller.h"
#include "Request.h"
#include "Response.h"

namespace Mongoose {
class RequestHandlerBase {
 public:
  virtual ~RequestHandlerBase() = default;
  virtual Response *process(Request &request) = 0;
};

template <typename T, typename R>
class RequestHandler : public RequestHandlerBase {
 public:
  typedef void (T::*fPtr)(Request &request, R &response);

  RequestHandler(T *controller_, const fPtr function_) : controller(controller_), function(function_) {}

  Response *process(Request &request) override {
    // Owned for the duration: every catch below used to return a different
    // object and leak this one, once per exception, for the life of the
    // process. Released to the caller on the way out, which is where
    // ownership transfers.
    std::unique_ptr<R> response(new R);

    try {
      (controller->*function)(request, *response);
    } catch (std::string &exception) {
      return Controller::internalErrorFromException(exception);
    } catch (const std::exception &exception) {
      return Controller::internalErrorFromException(exception.what());
    } catch (...) {
      return Controller::internalErrorFromException("Unknown error");
    }

    return response.release();
  }

 protected:
  T *controller;
  fPtr function;
};
}  // namespace Mongoose

#endif
