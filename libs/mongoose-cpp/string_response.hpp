#pragma once

#include <sstream>

#include "Response.h"
#include "dll_defines.hpp"

/**
 * A stream response to a request
 */
namespace mcp {
class NSCAPI_EXPORT string_response : public Mongoose::Response {
 private:
  std::string data;
  int response_code;

 public:
  string_response();
  string_response(int response_code, std::string data);

  std::string getBody() override;
  int get_response_code() const override;
  void set(std::string data);
};
}  // namespace mcp
