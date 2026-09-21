// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "grant_store.hpp"

#include <str/utils.hpp>

void grant_store::add_role(const std::string &role, const std::string &grant) {
  for (const std::string &g : str::utils::split<std::list<std::string> >(grant, ",")) {
    roles[role].rules.push_back(g);
  }
}

void grant_store::add_user(const std::string &user, const std::string &role) { users[user] = role; }

std::string grant_store::get_role(const std::string &user) const {
  const auto it = users.find(user);
  return it == users.end() ? std::string() : it->second;
}

void grant_store::remove_role(const std::string &role) { roles.erase(role); }

void grant_store::remove_user(const std::string &uid) { users.erase(uid); }

void grant_store::clear() {
  roles.clear();
  users.clear();
}

bool grant_store::validate(const std::string &uid, const std::string &check) const {
  std::list<std::string> need = str::utils::split_lst(check, ".");
  const grants g = fetch_role(uid);
  for (const std::string &rule : g.rules) {
    std::list<std::string> tokens = str::utils::split_lst(rule, ".");
    if (validate_grants(tokens, need)) {
      return true;
    }
  }
  return false;
}

grants grant_store::fetch_role(const std::string &uid) const {
  // find(), not the subscript: `users[uid]` inserted an empty role for every
  // uid it was ever asked about, so each permission check for an unknown user
  // grew the map for the lifetime of the process - and made a lookup mutate
  // the store, which is why this could not be const.
  const std::string role = get_role(uid);
  if (role.empty()) {
    return {};
  }
  const auto it = roles.find(role);
  return it == roles.end() ? grants() : it->second;
}

bool grant_store::validate_grants(std::list<std::string> &grant, std::list<std::string> &need) {
  grant_list::const_iterator grant_it = grant.begin();
  grant_list::const_iterator need_it = need.begin();
  while (need_it != need.end()) {
    if (grant_it == grant.end()) {
      return false;
    }
    if (*grant_it == "*") {
      return true;
    }
    if (*need_it != *grant_it) {
      return false;
    }
    ++grant_it;
    ++need_it;
  }
  return grant_it == grant.end();
}
