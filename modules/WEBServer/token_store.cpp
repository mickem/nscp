#include "token_store.hpp"

static const char alphanum[] = "0123456789" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz";

std::string token_store::generate_token(int len) {
  std::string ret;
  for (int i = 0; i < len; i++)
    ret += alphanum[rand() % (sizeof(alphanum) - 1)];
  return ret;
}

bool token_store::can(std::string uid, std::string grant) {
	return grants.validate(uid, grant);
}

void token_store::add_user(std::string user, std::string role) {
	grants.add_user(user, role);
}

void token_store::add_grant(std::string role, std::string grant) {
	grants.add_role(role, grant);
}

