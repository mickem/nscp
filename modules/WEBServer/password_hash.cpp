#include "password_hash.hpp"

#ifdef USE_SSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <str/constant_time.hpp>
#include <vector>

namespace web_password {
namespace {
constexpr int kPbkdf2Iterations = 100000;
constexpr int kPbkdf2HashBytes = 32;
constexpr int kPbkdf2SaltBytes = 16;
constexpr const char* kPbkdf2Prefix = "pbkdf2-sha256$";

std::string to_hex(const unsigned char* data, std::size_t len) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < len; ++i) {
    oss << std::setw(2) << static_cast<int>(data[i]);
  }
  return oss.str();
}

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
}  // namespace

bool is_hashed(const std::string& s) { return s.compare(0, std::strlen(kPbkdf2Prefix), kPbkdf2Prefix) == 0; }

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
  oss << kPbkdf2Prefix << kPbkdf2Iterations << "$" << to_hex(salt, sizeof(salt)) << "$" << to_hex(out, sizeof(out));
  return oss.str();
}

bool verify_password(const std::string& password, const std::string& stored) {
  if (!is_hashed(stored)) {
    return str::constant_time_eq(password, stored);
  }
  const std::string body = stored.substr(std::strlen(kPbkdf2Prefix));
  const auto p1 = body.find('$');
  if (p1 == std::string::npos) return false;
  const auto p2 = body.find('$', p1 + 1);
  if (p2 == std::string::npos) return false;
  const int iter = std::atoi(body.substr(0, p1).c_str());
  if (iter <= 0 || iter > 1000000) return false;
  std::vector<unsigned char> salt;
  std::vector<unsigned char> expected;
  if (!from_hex(body.substr(p1 + 1, p2 - p1 - 1), salt)) return false;
  if (!from_hex(body.substr(p2 + 1), expected)) return false;
  if (expected.empty() || salt.empty()) return false;
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
}  // namespace web_password
