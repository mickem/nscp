#include "Helpers.h"

#include <bytes/base64.h>
#include <bytes/base64.hpp>

#include <string>

namespace Mongoose {

std::string Helpers::encode_b64(const std::string &str) { return bytes::base64_encode(str); }

std::string Helpers::decode_b64(const std::string &str) {
  if (str.empty()) return {};
  const std::size_t needed = b64::b64_decode(str.data(), str.size(), nullptr, 0);
  // needed == 0 means invalid base64 (e.g. length not a multiple of 4). Bail
  // out before sizing `out` so we never take `&out[0]` on an empty string.
  if (needed == 0) return {};
  std::string out(needed, '\0');
  const std::size_t written = b64::b64_decode(str.data(), str.size(), &out[0], needed);
  out.resize(written);
  return out;
}

}  // namespace Mongoose
