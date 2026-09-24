// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscp/password_hash.hpp>

#include <nscp/sha256.hpp>

#ifdef USE_SSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <str/constant_time.hpp>
#include <vector>

namespace password_hash {
namespace {
constexpr int kPbkdf2Iterations = 100000;
constexpr int kPbkdf2HashBytes = 32;
constexpr int kPbkdf2SaltBytes = 16;
constexpr const char* kPbkdf2Prefix = "pbkdf2-sha256$";

bool from_hex(const std::string& hex, std::vector<unsigned char>& out) {
  if (hex.size() % 2 != 0) return false;
  out.clear();
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    unsigned int byte = 0;
    if (std::sscanf(hex.c_str() + i, "%2x", &byte) != 1) {
      return false;
    }
    out.push_back(static_cast<unsigned char>(byte));
  }
  return true;
}

// The prefix alone. Only verify_password() asks this: it is what separates "a
// hash, possibly a damaged one" from "a clear-text password", and a damaged
// hash must not fall through to the clear-text compare.
bool has_prefix(const std::string& s) { return s.compare(0, std::strlen(kPbkdf2Prefix), kPbkdf2Prefix) == 0; }

// Splits a stored value into its three fields. False unless every one of them
// is there and well formed, so a caller that gets true can verify against it.
// is_hashed() and verify_password() both go through here rather than each
// deciding for itself what counts as a hash.
bool parse_hash(const std::string& stored, int& iter, std::vector<unsigned char>& salt, std::vector<unsigned char>& expected) {
  if (!has_prefix(stored)) return false;
  const std::string body = stored.substr(std::strlen(kPbkdf2Prefix));
  const auto p1 = body.find('$');
  if (p1 == std::string::npos) return false;
  const auto p2 = body.find('$', p1 + 1);
  if (p2 == std::string::npos) return false;
  const std::string iterations = body.substr(0, p1);
  // Digits only, and short enough that atoi() cannot overflow before the range
  // check below gets a chance to reject the value (1000000 is seven digits).
  if (iterations.empty() || iterations.size() > 7) return false;
  if (iterations.find_first_not_of("0123456789") != std::string::npos) return false;
  iter = std::atoi(iterations.c_str());
  if (iter <= 0 || iter > 1000000) return false;
  if (!from_hex(body.substr(p1 + 1, p2 - p1 - 1), salt)) return false;
  if (!from_hex(body.substr(p2 + 1), expected)) return false;
  return !salt.empty() && !expected.empty();
}
}  // namespace

bool is_hashed(const std::string& s) {
  int iter = 0;
  std::vector<unsigned char> salt;
  std::vector<unsigned char> expected;
  return parse_hash(s, iter, salt, expected);
}

#ifdef USE_SSL
std::string hash_password(const std::string& password) {
  unsigned char salt[kPbkdf2SaltBytes];
  if (RAND_bytes(salt, sizeof(salt)) != 1) {
    return std::string();
  }
  unsigned char out[kPbkdf2HashBytes];
  if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt, sizeof(salt), kPbkdf2Iterations, EVP_sha256(), sizeof(out), out) != 1) {
    return std::string();
  }
  std::ostringstream oss;
  oss << kPbkdf2Prefix << kPbkdf2Iterations << "$" << web_digest::to_hex(salt, sizeof(salt)) << "$" << web_digest::to_hex(out, sizeof(out));
  return oss.str();
}

bool verify_password(const std::string& password, const std::string& stored) {
  int iter = 0;
  std::vector<unsigned char> salt;
  std::vector<unsigned char> expected;
  if (!parse_hash(stored, iter, salt, expected)) {
    // A value carrying the prefix that does not parse is a damaged hash, not a
    // password: fail rather than compare the stored string as clear text,
    // which would turn it into a credential of its own.
    if (has_prefix(stored)) return false;
    return str::constant_time_eq(password, stored);
  }
  std::vector<unsigned char> got(expected.size());
  if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.data(), static_cast<int>(salt.size()), iter, EVP_sha256(),
                        static_cast<int>(got.size()), got.data()) != 1) {
    return false;
  }
  return str::constant_time_eq(std::string(got.begin(), got.end()), std::string(expected.begin(), expected.end()));
}
#else
std::string hash_password(const std::string& password) { return password; }
bool verify_password(const std::string& password, const std::string& stored) { return str::constant_time_eq(password, stored); }
#endif
}  // namespace password_hash
