#pragma once

#include <algorithm>
#include <cstddef>
#include <string>

namespace str {
// Compares two strings in time independent of where the first differing byte is.
// Length difference is included in the diff so a length mismatch does not short
// circuit. Use for password / token / MAC equality checks.
inline bool constant_time_eq(const std::string& a, const std::string& b) {
  unsigned char diff = static_cast<unsigned char>((a.size() ^ b.size()) & 0xff);
  const std::size_t n = std::max(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    const unsigned char ca = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
    const unsigned char cb = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
    diff |= static_cast<unsigned char>(ca ^ cb);
  }
  return diff == 0 && a.size() == b.size();
}
}  // namespace str
