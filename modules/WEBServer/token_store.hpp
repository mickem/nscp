// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/unordered_set.hpp>
#include <ctime>
#include <mutex>
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

  // Guards `tokens` and `grants`. Both web backends currently serve on a
  // single thread, so this is not load-bearing today - but nothing in the
  // class enforced that, every sibling store in this module (error_handler,
  // event_store, metrics_handler, auth_rate_limiter) is locked, and the
  // Beast backend is one line away from a multi-threaded ioc_.run(). A
  // session table silently corrupting the first time someone adds a worker
  // pool is not a good failure mode to leave armed.
  mutable std::mutex mutex_;

  token_map tokens;
  grant_store grants;

  // Sweep all expired entries plus, when the map is at the cap, the oldest
  // entries until we're back under it. Called from generate_for so every
  // new login covers its own bookkeeping cost - no separate timer thread.
  // Caller must hold mutex_ (hence the name).
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
  // Signature of OpenSSL's RAND_bytes. Only used for the test seam below.
  using rand_bytes_fn = int (*)(unsigned char *buf, int num);

  // Test seam: replaces the CSPRNG backing generate_token(). Passing nullptr
  // restores the real one. Never called in production - it exists so the
  // fail-closed path (CSPRNG present but failing) and the rejection-sampling
  // loop can be exercised deterministically.
  static void set_rand_bytes_for_test(rand_bytes_fn fn);

  // Returns an empty string if a CSPRNG is present but failed; callers MUST
  // treat that as a failure to issue a credential rather than proceeding.
  static std::string generate_token(int len);
  static time_t now() { return time(nullptr); }

  bool is_valid(const std::string &token) { return is_valid(token, now()); }

  bool is_valid(const std::string &token, const time_t now) {
    const std::lock_guard<std::mutex> lock(mutex_);
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

  // Resolve validity and identity under a single lock and a single clock
  // read. is_valid() followed by get_user() takes the lock twice and calls
  // now() twice, so a token expiring between the two calls yielded an empty
  // uid on an otherwise-authorised request. Callers on the request path
  // should use this; is_valid()/get_user() remain for callers that need only
  // one half.
  bool validate(const std::string &token, std::string &user) { return validate(token, user, now()); }

  bool validate(const std::string &token, std::string &user, const time_t now) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tokens.find(token);
    if (it == tokens.end()) return false;
    if (has_token_expired(it->second.created, now)) {
      tokens.erase(it);
      return false;
    }
    user = it->second.user;
    return true;
  }

  std::string get_user(const std::string &token) const { return get_user(token, now()); }

  std::string get_user(const std::string &token, const time_t now) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto cit = tokens.find(token);
    if (cit != tokens.end() && !has_token_expired(cit->second.created, now)) {
      return cit->second.user;
    }
    // Do not resolve a username for an expired (or missing) token. is_valid()
    // is the primary gate and evicts on lookup, but guarding here too keeps the
    // "expired token never names a user" invariant even if a caller reads
    // get_user() without a preceding is_valid().
    return "";
  }

  std::string generate_for(const std::string &user) {
    // Generate before taking the lock: the CSPRNG call does not need it, and
    // an empty result means the CSPRNG failed, in which case no session may
    // be created at all. Storing a "" key would hand every tokenless request
    // a valid session.
    std::string token = generate_token(32);
    if (token.empty()) return token;
    const time_t t = now();
    const std::lock_guard<std::mutex> lock(mutex_);
    sweep_expired_locked(t);
    token_entry entry;
    entry.user = user;
    entry.created = t;
    tokens[token] = entry;
    return token;
  }

  void revoke(const std::string &token) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tokens.find(token);
    if (it != tokens.end()) {
      tokens.erase(it);
    }
  }
  void revoke_tokens_for_user(const std::string &user) {
    const std::lock_guard<std::mutex> lock(mutex_);
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
