#include "Helpers.h"

#include <bytes/base64.h>
#include <bytes/base64.hpp>

#include <string>

namespace Mongoose {

std::string Helpers::encode_b64(const std::string &str) { return bytes::base64_encode(str); }

std::string Helpers::decode_b64(const std::string &str) {
  if (str.empty()) return {};
  const std::size_t needed = b64::b64_decode(str.data(), str.size(), nullptr, 0);
  std::string out(needed, '\0');
  const std::size_t written = b64::b64_decode(str.data(), str.size(), &out[0], needed);
  out.resize(written);
  return out;
}

}  // namespace Mongoose
