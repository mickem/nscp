// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_fingerprint.hpp"

namespace icinga_agent {

namespace {
constexpr std::size_t kSha256HexDigits = 64;

bool is_separator(const char c) { return c == ':' || c == ' ' || c == '\t'; }

// The lower-case hex digit for `c`, or 0 when it is not one.
char hex_digit(const char c) {
  if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) return c;
  if (c >= 'A' && c <= 'F') return static_cast<char>(c - 'A' + 'a');
  return 0;
}
}  // namespace

bool normalize_fingerprint(const std::string &text, std::string &out) {
  // "SHA256 Fingerprint=AB:CD:..." - keep what follows the '='.
  const std::size_t eq = text.find('=');
  const std::string value = eq == std::string::npos ? text : text.substr(eq + 1);
  std::string hex;
  for (const char c : value) {
    if (is_separator(c)) continue;
    const char digit = hex_digit(c);
    if (digit == 0) return false;
    hex += digit;
  }
  if (hex.size() != kSha256HexDigits) return false;
  out = hex;
  return true;
}

}  // namespace icinga_agent
