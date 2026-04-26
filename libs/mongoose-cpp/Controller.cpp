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
