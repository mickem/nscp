// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_USER_MANAGER_H
#define NSCP_USER_MANAGER_H
#include <boost/unordered/unordered_map.hpp>
#include <string>

class user_manager {
  boost::unordered_map<std::string, std::string> users;

 public:
  bool validate_user(const std::string &user, const std::string &password);
  void add_user(const std::string &user, const std::string &password);
  bool has_user(const std::string &user) const;
  // The stored password value for a user (the PBKDF2 string, or the legacy
  // plaintext where hashing was unavailable), or "" when the user is unknown.
  // Used to bind a session to the credentials it was issued against - never
  // to make an authentication decision, which goes through validate_user.
  std::string get_hash(const std::string &user) const;
  void remove_user(const std::string &user);
};

#endif  // NSCP_USER_MANAGER_H
