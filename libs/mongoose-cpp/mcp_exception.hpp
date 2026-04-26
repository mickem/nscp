#pragma once

#include "dll_defines.hpp"
#ifdef WIN32
#pragma warning(disable : 4251)
#endif

#include <string>
#include <utility>

namespace mcp {

class NSCAPI_EXPORT mcp_exception {
  std::string error;

 public:
  explicit mcp_exception(std::string error) : error(std::move(error)) {};
  ~mcp_exception() noexcept = default;

  const char* what() const noexcept { return error.c_str(); }
  std::string reason() const noexcept { return error; }
};

}  // namespace mcp