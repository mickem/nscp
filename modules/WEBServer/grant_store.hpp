// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/unordered_map.hpp>
#include <list>
#include <string>

struct grants {
  std::list<std::string> rules;
};
class grant_store {
  boost::unordered_map<std::string, grants> roles;
  boost::unordered_map<std::string, std::string> users;

 public:
  void add_role(const std::string &role, const std::string &grant);
  void add_user(const std::string &user, const std::string &role);
  // The role a user is mapped to, or "" when the user has none. Read-only
  // twin of add_user: the session layer needs the role to build a user's
  // credential fingerprint, and a lookup is cheaper (and cannot drift) than
  // a second copy of the mapping.
  std::string get_role(const std::string &user) const;
  void remove_role(const std::string &role);
  void remove_user(const std::string &uid);
  void clear();

  bool validate(const std::string &uid, const std::string &check);

 private:
  typedef std::list<std::string> grant_list;
  grants fetch_role(const std::string &uid);
  static bool validate_grants(grant_list &grant, grant_list &need);
};