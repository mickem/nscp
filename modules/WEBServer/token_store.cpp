#include "token_store.hpp"

#include <random>

static constexpr char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

std::string token_store::generate_token(const int len) {
  constexpr std::size_t alphanum_size = sizeof(alphanum) - 1;
  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, static_cast<int>(alphanum_size) - 1);
  std::string ret;
  ret.reserve(len);
  for (int i = 0; i < len; i++) ret += alphanum[dist(rd)];
  return ret;
}

bool token_store::can(const std::string &uid, const std::string &grant) { return grants.validate(uid, grant); }

void token_store::add_user(const std::string &user, const std::string &role) { grants.add_user(user, role); }

void token_store::add_grant(const std::string &role, const std::string &grant) { grants.add_role(role, grant); }
