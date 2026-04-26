#ifndef _MONGOOSE_STREAM_RESPONSE_H
#define _MONGOOSE_STREAM_RESPONSE_H

#include <sstream>

#include "Response.h"
#include "dll_defines.hpp"

/**
 * A stream response to a request
 */
namespace Mongoose {
class NSCAPI_EXPORT StreamResponse : public Response {
  std::stringstream ss;
  int response_code;

 public:
  StreamResponse() : response_code(0) {}
  explicit StreamResponse(const int response_code) : response_code(response_code) {}
  /**
   * Gets the response body
   *
   * @return string the response body
   */
  std::string getBody() override;
  int get_response_code() const override { return response_code; }
  void append(const std::string& data);
  void write(const char* buffer, std::size_t len);
  void setCodeServerError(const std::string& msg);
  void setCodeNotFound(const std::string& msg);
  void setCodeForbidden(const std::string& msg);
  void setCodeBadRequest(const std::string& msg);
};
}  // namespace Mongoose

#endif
