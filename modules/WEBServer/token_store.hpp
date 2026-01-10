#pragma once

#include <boost/unordered_set.hpp>
#include <ctime>
#include <string>
#include <utility>

#include "grant_store.hpp"

#define TOKEN_EXPIRATION_HOURS 8
#define HOURS_TO_SECONDS(h) ((h) * 60 * 60)

inline bool has_token_expired(const time_t created_time, const time_t now) {
  return created_time > now || now > created_time + HOURS_TO_SECONDS(TOKEN_EXPIRATION_HOURS);
}
class token_store {
  struct token_entry {
    std::string user;
    time_t created{};
  };
  typedef boost::unordered_map<std::string, token_entry> token_map;

  token_map tokens;
  grant_store grants;

 public:
  static std::string generate_token(int len);
  static time_t now() { return time(nullptr); }

  bool is_valid(const std::string &token) { return is_valid(token, now()); }

  bool is_valid(const std::string &token, const time_t now) {
    const auto it = tokens.find(token);
    if (it == tokens.end()) {
      return false;
    }
    if (has_token_expired(it->second.created, now)) {
      tokens.erase(it);
      return false;
    }
    return true;
  }

  std::string get_user(const std::string &token) const {
    const auto cit = tokens.find(token);
    if (cit != tokens.end()) {
      return cit->second.user;
    }
    return "";
  }

  std::string generate_for(const std::string &user) {
    std::string token = generate_token(32);
    token_entry entry;
    entry.user = user;
    entry.created = now();
    tokens[token] = entry;
    return token;
  }

  void revoke(const std::string &token) {
    const auto it = tokens.find(token);
    if (it != tokens.end()) {
      tokens.erase(it);
    }
  }
  bool can(const std::string &uid, const std::string &grant);
  void add_user(const std::string &user, const std::string &role);
  void add_grant(const std::string &role, const std::string &grant);
};
