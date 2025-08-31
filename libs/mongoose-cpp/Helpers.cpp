#include "Helpers.h"

#include <mongoose.h>

#include <char_buffer.hpp>

/**
 * A stream response to a request
 */
namespace Mongoose {
std::string Helpers::encode_b64(const std::string &str) {
  const hlp::char_buffer dst((str.size() * 3) + 3);
  const hlp::char_buffer src(str);
  size_t encoded_len = mg_base64_encode(src.get_t<unsigned char *>(), static_cast<int>(str.size()), dst.get(), dst.size());
  return std::string{dst.get(), encoded_len};
}
std::string Helpers::decode_b64(const std::string &str) {
  const hlp::char_buffer dst(str.size() * 3);
  const hlp::char_buffer src(str);
  size_t decoded_len = mg_base64_decode(src.get_t<char *>(), static_cast<int>(str.size()), dst.get(), dst.size());
  return std::string{dst.get(), decoded_len};
}
}  // namespace Mongoose
