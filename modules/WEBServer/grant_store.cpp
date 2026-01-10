#include "grant_store.hpp"

#include <str/utils.hpp>

void grant_store::add_role(const std::string &role, const std::string &grant) {
  for (const std::string &g : str::utils::split<std::list<std::string> >(grant, ",")) {
    roles[role].rules.push_back(g);
  }
}

void grant_store::add_user(const std::string &user, const std::string &role) { users[user] = role; }

void grant_store::remove_role(const std::string &role) { roles.erase(role); }

void grant_store::remove_user(const std::string &uid) { users.erase(uid); }

void grant_store::clear() {
  roles.clear();
  users.clear();
}

bool grant_store::validate(const std::string &uid, const std::string &check) {
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

grants grant_store::fetch_role(const std::string &uid) {
  const std::string role = users[uid];
  if (role.empty()) {
    return {};
  }
  return roles[role];
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
