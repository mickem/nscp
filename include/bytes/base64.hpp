// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <bytes/base64.h>

#include <string>

namespace bytes {

inline std::string base64_encode(const void *data, std::size_t size) {
  if (size == 0) return {};
  const std::size_t len = b64::b64_encode(data, size, nullptr, 0);
  std::string out(len, '\0');
  b64::b64_encode(data, size, &out[0], len);
  return out;
}

inline std::string base64_encode(const std::string &input) { return base64_encode(input.data(), input.size()); }

// Decode standard base64, skipping ASCII whitespace so a value wrapped across
// lines (PEM bodies, hand-edited config) decodes as written. Empty for empty
// input and for anything that is not valid base64: callers that must tell the
// two apart check the input first.
inline std::string base64_decode(const std::string &encoded) {
  std::string compact;
  compact.reserve(encoded.size());
  for (const char c : encoded) {
    if (c != '\n' && c != '\r' && c != ' ' && c != '\t') compact.push_back(c);
  }
  if (compact.empty()) return {};
  const std::size_t needed = b64::b64_decode(compact.data(), compact.size(), nullptr, 0);
  if (needed == 0) return {};
  std::string out(needed, '\0');
  const std::size_t written = b64::b64_decode(compact.data(), compact.size(), &out[0], needed);
  if (written == 0) return {};
  out.resize(written);
  return out;
}

}  // namespace bytes
