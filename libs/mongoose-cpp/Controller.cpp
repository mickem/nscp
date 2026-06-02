// SPDX-FileCopyrightText: 2013 Grégoire Passault
// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: MIT

#include "Controller.h"

#include "StreamResponse.h"

namespace Mongoose {

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
