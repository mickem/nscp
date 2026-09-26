// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <string>

#ifdef USE_SSL
#include <openssl/evp.h>
#endif

// The agent's one SHA-256-and-hex implementation. Three call sites used to
// carry a hand-rolled copy - token_store (the session-table key),
// web_installer (the bundle integrity check) and password_hash (the PBKDF2
// salt and hash encoding) - each with its own error handling and its own
// ostringstream hex loop. A digest or encoding fix now lands in one place.
//
// It lives next to password_hash.hpp because that one is built into the
// NSCAServer and NSClientServer modules too, which have no access to the WEB
// module's sources.
//
// Header-only and deliberately dependency-free (<string> plus OpenSSL): it is
// included by token_store.cpp and password_hash.cpp, which the WEBServer unit
// test builds on their own, without the module's Mongoose/protobuf surface.
namespace web_digest {

// Lowercase hex of the `len` bytes at `data`. Table-driven rather than
// ostringstream: this sits on the per-request session-lookup path, where a
// stream construction, a locale lookup and a setw reformat per byte are all
// overhead.
inline std::string to_hex(const unsigned char *data, const std::size_t len) {
  static const char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out.push_back(digits[data[i] >> 4]);
    out.push_back(digits[data[i] & 0x0f]);
  }
  return out;
}

// True when this build has a digest at all. sha256_hex() returns "" when it
// is false, and callers that must fail closed key off this rather than
// guessing from an empty result.
inline bool has_sha256() {
#ifdef USE_SSL
  return true;
#else
  return false;
#endif
}

// SHA-256 of `in` as lowercase hex, or "" when the digest failed or this
// build has none. Non-throwing: the session path has to fail closed on a
// value rather than an exception, and the one caller that wants to throw
// (web_installer) tests for "" itself.
inline std::string sha256_hex(const std::string &in) {
#ifdef USE_SSL
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int md_len = 0;
  // One-shot: EVP_Digest does the init/update/final internally, so there is
  // no context to allocate, to free, or to leak on an early return.
  if (EVP_Digest(in.data(), in.size(), md, &md_len, EVP_sha256(), nullptr) != 1) return std::string();
  return to_hex(md, md_len);
#else
  (void)in;
  return std::string();
#endif
}

}  // namespace web_digest
