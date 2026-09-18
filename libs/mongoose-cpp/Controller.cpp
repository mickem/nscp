// SPDX-FileCopyrightText: 2013 Grégoire Passault
// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: MIT

#include "Controller.h"

#include <utility>

#include "StreamResponse.h"

namespace Mongoose {

namespace {
Controller::error_sink& sink() {
  static Controller::error_sink instance;
  return instance;
}
}  // namespace

void Controller::setErrorSink(error_sink new_sink) { sink() = std::move(new_sink); }

Response* Controller::internalErrorFromException(const std::string& detail) {
  if (sink()) sink()("Unhandled exception while handling a request: " + detail);
  auto* response = new StreamResponse;
  // Deliberately without the detail: the caller learns that the request
  // failed, the operator learns why, from the log.
  response->setCodeServerError("[500] Server internal error");
  return response;
}

Response* Controller::serverInternalError(const std::string& message) {
  auto* response = new StreamResponse;

  response->setCodeServerError("[500] Server internal error: " + message);

  return response;
}
Response* Controller::documentMissing(const std::string& message) {
  auto* response = new StreamResponse;

  response->setCodeNotFound("[500] Document not found: " + message);

  return response;
}
}  // namespace Mongoose
