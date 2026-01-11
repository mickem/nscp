#include "token_store.hpp"

static constexpr char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

std::string token_store::generate_token(const int len) {
  std::string ret;
  for (int i = 0; i < len; i++) ret += alphanum[rand() % (sizeof(alphanum) - 1)];
  return ret;
}

bool token_store::can(const std::string &uid, const std::string &grant) { return grants.validate(uid, grant); }

void token_store::add_user(const std::string &user, const std::string &role) { grants.add_user(user, role); }

void token_store::add_grant(const std::string &role, const std::string &grant) { grants.add_role(role, grant); }
