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

}  // namespace bytes
