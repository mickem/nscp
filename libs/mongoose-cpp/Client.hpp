#pragma once

#include <boost/shared_ptr.hpp>
#include <map>
#include <string>

#include "Response.h"
#include "dll_defines.hpp"

/**
 * Wrapper for the Mongoose server
 */
namespace Mongoose {
class NSCAPI_EXPORT Client final {
 public:
  typedef std::map<std::string, std::string> header_type;
  /**
   * Constructs the server
   *
   * @param url The url
   */
  explicit Client(std::string url);
  ~Client();
  boost::shared_ptr<Response> fetch(std::string verb, header_type hdr, std::string payload) const;

 private:
  std::string url_;
};
}  // namespace Mongoose
