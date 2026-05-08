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

  // Hard cap on the number of live tokens. The previous implementation only
  // pruned on lookup, so a stream of "/api/v2/login" + abandoned-session
  // workloads grew the map without bound. 4096 is generous - real
  // deployments rarely have more than a handful of concurrent admin
  // sessions, and the cap is a defensive boundary, not a tuned limit.
  static constexpr std::size_t kMaxTokens = 4096;

  token_map tokens;
  grant_store grants;

  // Sweep all expired entries plus, when the map is at the cap, the oldest
  // entries until we're back under it. Called from generate_for so every
  // new login covers its own bookkeeping cost - no separate timer thread,
  // no shared state to race against.
  void sweep_expired_locked(const time_t now) {
    for (auto it = tokens.begin(); it != tokens.end();) {
      if (has_token_expired(it->second.created, now)) {
        it = tokens.erase(it);
      } else {
        ++it;
      }
    }
    if (tokens.size() < kMaxTokens) return;
    // Still over the cap after expiring stale entries. Drop the oldest
    // tokens (longest-lived sessions) until we're back under. This is O(N)
    // but only runs when the map is full, which means N is bounded by
    // kMaxTokens.
    while (tokens.size() >= kMaxTokens) {
      auto oldest = tokens.begin();
      for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it->second.created < oldest->second.created) oldest = it;
      }
      if (oldest == tokens.end()) break;
      tokens.erase(oldest);
    }
  }

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
    const time_t t = now();
    sweep_expired_locked(t);
    std::string token = generate_token(32);
    token_entry entry;
    entry.user = user;
    entry.created = t;
    tokens[token] = entry;
    return token;
  }

  void revoke(const std::string &token) {
    const auto it = tokens.find(token);
    if (it != tokens.end()) {
      tokens.erase(it);
    }
  }
  void revoke_tokens_for_user(const std::string &user) {
    for (auto it = tokens.begin(); it != tokens.end();) {
      if (it->second.user == user) {
        it = tokens.erase(it);
      } else {
        ++it;
      }
    }
  }
  bool can(const std::string &uid, const std::string &grant);
  void add_user(const std::string &user, const std::string &role);
  void add_grant(const std::string &role, const std::string &grant);
};
