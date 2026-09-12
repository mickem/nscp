// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <onboarding/onboarding.hpp>
#include <openssl/evp.h>
#include <string>

// SHA-256 over a buffer and hex encoding of the result: the two helpers the
// onboarding library needs in more than one translation unit (verify.cpp
// digests a bundle, bundle_crypto.cpp fingerprints a key). Internal to
// libs/onboarding - the public headers expose sha256_hex() instead, which is
// these two composed.
namespace onboarding {
namespace detail {

inline std::string sha256_raw(const std::string &bytes) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int length = 0;
  if (EVP_Digest(bytes.data(), bytes.size(), digest, &length, EVP_sha256(), nullptr) != 1) {
    throw onboarding_error("SHA-256 digest failed", false);
  }
  return std::string(reinterpret_cast<const char *>(digest), length);
}

inline std::string to_hex(const std::string &bytes) {
  static const char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (const char c : bytes) {
    const auto b = static_cast<unsigned char>(c);
    out.push_back(digits[b >> 4]);
    out.push_back(digits[b & 0x0f]);
  }
  return out;
}

}  // namespace detail
}  // namespace onboarding
