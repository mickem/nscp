// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "token_store.hpp"

#include <random>
#include <vector>

#ifdef USE_SSL
#include <openssl/rand.h>
#endif

static constexpr char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

namespace {
// Fallback generator used only when no CSPRNG is available (USE_SSL off) or
// RAND_bytes fails. std::random_device is NOT guaranteed by the standard to be
// non-deterministic - some toolchains (historically MinGW-w64) implement it as
// a fixed-seed PRNG, which would make session tokens predictable. It is kept
// solely so a build without OpenSSL still produces *some* token rather than an
// empty string.
std::string generate_token_fallback(const int len, const std::size_t alphanum_size) {
  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, static_cast<int>(alphanum_size) - 1);
  std::string ret;
  ret.reserve(len);
  for (int i = 0; i < len; i++) ret += alphanum[dist(rd)];
  return ret;
}
}  // namespace

std::string token_store::generate_token(const int len) {
  constexpr std::size_t alphanum_size = sizeof(alphanum) - 1;
  if (len <= 0) return std::string();
#ifdef USE_SSL
  // Session tokens are the primary bearer credential, so draw them from the
  // same cryptographic RNG the module already links for password salts
  // (password_hash.cpp). Reject bytes at or above the largest multiple of the
  // alphabet size so the modulo mapping stays unbiased (256 % 62 != 0).
  const unsigned int reject_limit = 256 - (256 % static_cast<unsigned int>(alphanum_size));
  std::string ret;
  ret.reserve(len);
  std::vector<unsigned char> buf(static_cast<std::size_t>(len));
  while (static_cast<int>(ret.size()) < len) {
    if (RAND_bytes(buf.data(), static_cast<int>(buf.size())) != 1) {
      // CSPRNG unavailable/failed - fall back rather than emit a short or
      // empty token that would weaken or break authentication.
      return generate_token_fallback(len, alphanum_size);
    }
    for (std::size_t i = 0; i < buf.size() && static_cast<int>(ret.size()) < len; ++i) {
      if (buf[i] < reject_limit) ret += alphanum[buf[i] % alphanum_size];
    }
  }
  return ret;
#else
  return generate_token_fallback(len, alphanum_size);
#endif
}

// `grants` is guarded by the same mutex as `tokens`: add_user / add_grant run
// from the settings load path while can() is on the per-request authorisation
// path, so they are not naturally serialised against each other.
bool token_store::can(const std::string &uid, const std::string &grant) {
  const std::lock_guard<std::mutex> lock(mutex_);
  return grants.validate(uid, grant);
}

void token_store::add_user(const std::string &user, const std::string &role) {
  const std::lock_guard<std::mutex> lock(mutex_);
  grants.add_user(user, role);
}

void token_store::add_grant(const std::string &role, const std::string &grant) {
  const std::lock_guard<std::mutex> lock(mutex_);
  grants.add_role(role, grant);
}
