// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/regex.hpp>
#include <memory>
#include <string>

#include "Controller.h"
#include "Request.h"
#include "Response.h"

namespace Mongoose {

class RegexpRequestHandlerBase {
 public:
  virtual ~RegexpRequestHandlerBase() = default;
  virtual Response *process(Request &request, boost::smatch &what) = 0;
};

template <typename T, typename R>
class RegexpRequestHandler : public RegexpRequestHandlerBase {
 public:
  typedef void (T::*fPtr)(Request &request, boost::smatch &what, R &response);

  RegexpRequestHandler(T *controller_, const fPtr function_) : controller(controller_), function(function_) {}

  Response *process(Request &request, boost::smatch &what) override {
    // Owned for the duration: every catch below used to return a different
    // object and leak this one, once per exception, for the life of the
    // process. Released to the caller on the way out, which is where
    // ownership transfers.
    std::unique_ptr<R> response(new R);

    try {
      (controller->*function)(request, what, *response);
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
