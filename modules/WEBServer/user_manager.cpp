#include "user_manager.h"

#include "password_hash.hpp"

namespace {
// Decoy hash used when the requested user does not exist. Verifying against
// this gives the failed-lookup branch the same wall-clock cost as a real
// PBKDF2 verification, removing a user-enumeration timing oracle.
constexpr const char* kDecoyHash =
    "pbkdf2-sha256$100000$00000000000000000000000000000000$"
    "0000000000000000000000000000000000000000000000000000000000000000";
}  // namespace

bool user_manager::validate_user(const std::string& user, const std::string& password) {
  if (password.empty()) {
    return false;
  }
  const auto it = users.find(user);
  if (it == users.end()) {
    // Decoy verify so failed-lookup latency matches a real verification.
    (void)web_password::verify_password(password, kDecoyHash);
    return false;
  }
  return web_password::verify_password(password, it->second);
}

void user_manager::add_user(const std::string& user, const std::string& password) {
  if (password.empty() || web_password::is_hashed(password)) {
    users[user] = password;
    return;
  }
  const std::string h = web_password::hash_password(password);
  users[user] = h.empty() ? password : h;
}

bool user_manager::has_user(const std::string& user) const { return users.find(user) != users.end(); }

void user_manager::remove_user(const std::string& user) { users.erase(user); }
