// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "user_manager.h"

#include <nscp/password_hash.hpp>

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
    (void)password_hash::verify_password(password, kDecoyHash);
    return false;
  }
  return password_hash::verify_password(password, it->second);
}

void user_manager::add_user(const std::string& user, const std::string& password) {
  if (password.empty() || password_hash::is_hashed(password)) {
    users[user] = password;
    return;
  }
  const std::string h = password_hash::hash_password(password);
  users[user] = h.empty() ? password : h;
}

bool user_manager::has_user(const std::string& user) const { return users.find(user) != users.end(); }

std::string user_manager::get_hash(const std::string& user) const {
  const auto it = users.find(user);
  return it == users.end() ? std::string() : it->second;
}

void user_manager::remove_user(const std::string& user) { users.erase(user); }
