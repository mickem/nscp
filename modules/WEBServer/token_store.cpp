#include "token_store.hpp"

static const char alphanum[] = "0123456789" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz";

std::string token_store::generate_token(int len) {
  std::string ret;
  for (int i = 0; i < len; i++)
    ret += alphanum[rand() % (sizeof(alphanum) - 1)];
  return ret;
}
