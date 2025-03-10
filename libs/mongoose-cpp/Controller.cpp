#include "Controller.h"

#include "StreamResponse.h"

namespace Mongoose {

Response* Controller::serverInternalError(std::string message) {
  StreamResponse* response = new StreamResponse;

  response->setCodeServerError("[500] Server internal error: " + message);

  return response;
}
Response* Controller::documentMissing(std::string message) {
  StreamResponse* response = new StreamResponse;

  response->setCodeNotFound("[500] Document not found: " + message);

  return response;
}
}  // namespace Mongoose
