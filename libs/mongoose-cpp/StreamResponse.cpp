#include "StreamResponse.h"

namespace Mongoose {
std::string StreamResponse::getBody() { return ss.str(); }

void StreamResponse::append(std::string data) { ss << data; }

void StreamResponse::write(const char* buffer, std::size_t len) { ss.write(buffer, len); }
void StreamResponse::setCodeServerError(const std::string& msg) {
  setCode(HTTP_SERVER_ERROR, REASON_SERVER_ERROR);
  append(msg);
}
void StreamResponse::setCodeNotFound(const std::string& msg) {
  setCode(HTTP_NOT_FOUND, REASON_NOT_FOUND);
  append(msg);
}
void StreamResponse::setCodeForbidden(const std::string& msg) {
  setCode(HTTP_FORBIDDEN, REASON_FORBIDDEN);
  append(msg);
}
void StreamResponse::setCodeBadRequest(const std::string& msg) {
  setCode(HTTP_BAD_REQUEST, REASON_BAD_REQUEST);
  append(msg);
}

}  // namespace Mongoose
