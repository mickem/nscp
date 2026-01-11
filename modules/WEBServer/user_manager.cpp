#include "user_manager.h"

bool user_manager::validate_user(const std::string &user, const std::string &password) {
  if (password.empty()) {
    return false;
  }
  if (users.find(user) == users.end()) {
    return false;
  }
  return users[user] == password;
}

void user_manager::add_user(const std::string &user, const std::string &password) { users[user] = password; }

bool user_manager::has_user(const std::string &user) const { return users.find(user) != users.end(); }
