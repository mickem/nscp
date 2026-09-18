// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/unordered_set.hpp>
#include <cstddef>
#include <ctime>
#include <list>
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
    // Binds the session to the credentials it was issued against: see
    // session_manager_interface::fingerprint_for_user. Empty for a token
    // issued through the fingerprint-less generate_for() overload, which is
    // then never persisted.
    std::string fingerprint;
    // Only a session the client was actually handed - the key returned by
    // GET /api/v2/login - is worth carrying across a restart. Every Basic-auth
    // request mints a token too (process_auth_header), and a monitoring
    // system polling with Basic auth mints thousands of them that nobody
    // ever sees again; persisting those would fill nsclient.db with sessions
    // no client holds. So a token is born volatile and snapshot() only exports
    // the ones marked through mark_persistent().
    bool persistent{false};
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
  // One live session, in the form that survives a restart. `hash` is the
  // map key (the SHA-256 of the raw token, hex); the raw token itself is
  // never held by this class, so it is not in here either.
  struct persisted_session {
    std::string hash;
    std::string user;
    time_t created{};
    std::string fingerprint;
  };

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

  // SHA-256 of `in`, lowercase hex. Empty when this build has no hash
  // function at all (USE_SSL off) or the digest failed. This is the one
  // hashing primitive the session layer uses: it turns a raw bearer token
  // into the map key stored here and on disk, and it is what
  // session_manager_interface folds the role and the stored password hash
  // into to build a credential fingerprint.
  static std::string hash_token(const std::string &in);

  // True when hash_token() can produce a hash in this build. A build without
  // one keys the map by the raw token (see key_for) and persists nothing -
  // it cannot serve TLS either, so there is no session worth carrying across
  // a restart.
  static bool has_hashing();

  // The map key for a raw token: its hash where this build has one, the raw
  // token otherwise. In a hashing build a failed digest yields "", which no
  // entry is ever stored under (generate_for refuses to mint in that case),
  // so the lookup simply misses. Public so tests can assert the map really is
  // keyed by the hash and not by the token.
  static std::string key_for(const std::string &token) { return has_hashing() ? hash_token(token) : token; }

  bool is_valid(const std::string &token) { return is_valid(token, now()); }

  bool is_valid(const std::string &token, const time_t now) {
    const std::string key = key_for(token);
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tokens.find(key);
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
    const std::string key = key_for(token);
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tokens.find(key);
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
    const std::string key = key_for(token);
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto cit = tokens.find(key);
    if (cit != tokens.end() && !has_token_expired(cit->second.created, now)) {
      return cit->second.user;
    }
    // Do not resolve a username for an expired (or missing) token. is_valid()
    // is the primary gate and evicts on lookup, but guarding here too keeps the
    // "expired token never names a user" invariant even if a caller reads
    // get_user() without a preceding is_valid().
    return "";
  }

  std::string generate_for(const std::string &user) { return generate_for(user, ""); }

  // Returns "" when no session could be minted: the CSPRNG failed, or (in a
  // hashing build) the digest did. The second case matters as much as the
  // first: storing the raw token under a key every later lookup would hash
  // would hand the client a credential that 403s until it expires and that
  // revoke() cannot find either. Callers treat "" as a failure to issue.
  std::string generate_for(const std::string &user, const std::string &fingerprint) {
    // Generate before taking the lock: the CSPRNG call does not need it, and
    // an empty result means the CSPRNG failed, in which case no session may
    // be created at all. Storing a "" key would hand every tokenless request
    // a valid session.
    std::string token = generate_token(32);
    if (token.empty()) return token;
    // Hash outside the lock too - it is pure, and the raw token is only ever
    // returned to the caller, never stored.
    const std::string key = key_for(token);
    if (key.empty()) return std::string();
    const time_t t = now();
    const std::lock_guard<std::mutex> lock(mutex_);
    sweep_expired_locked(t);
    token_entry entry;
    entry.user = user;
    entry.created = t;
    entry.fingerprint = fingerprint;
    tokens[key] = entry;
    return token;
  }

  // Flag a live session as one the client holds (see token_entry::persistent).
  // Returns false when the token is unknown or expired.
  bool mark_persistent(const std::string &token) { return mark_persistent(token, now()); }

  bool mark_persistent(const std::string &token, const time_t now) {
    const std::string key = key_for(token);
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tokens.find(key);
    if (it == tokens.end() || has_token_expired(it->second.created, now)) return false;
    it->second.persistent = true;
    return true;
  }

  // Every live, unexpired session that may leave the process: the ones a
  // client was handed (mark_persistent). A build with no hash function keys
  // the map by the raw token, and exporting that would write the bearer
  // credential itself to disk, so it exports nothing at all.
  std::list<persisted_session> snapshot(const time_t now) const {
    std::list<persisted_session> ret;
    if (!has_hashing()) return ret;
    const std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &e : tokens) {
      if (!e.second.persistent) continue;
      if (has_token_expired(e.second.created, now)) continue;
      persisted_session s;
      s.hash = e.first;
      s.user = e.second.user;
      s.created = e.second.created;
      s.fingerprint = e.second.fingerprint;
      ret.push_back(s);
    }
    return ret;
  }

  // Put a previously exported session back. Returns whether it was inserted.
  // Rejects an expired record, and never overwrites a live entry - a session
  // minted since boot is newer than anything on disk.
  bool restore(const persisted_session &session, const time_t now) {
    if (session.hash.empty() || session.user.empty()) return false;
    if (has_token_expired(session.created, now)) return false;
    const std::lock_guard<std::mutex> lock(mutex_);
    if (tokens.find(session.hash) != tokens.end()) return false;
    sweep_expired_locked(now);
    // sweep_expired_locked only drops down to kMaxTokens - 1, so there is
    // room for exactly one more. Re-check anyway: a caller importing a whole
    // file must not push the map past the cap.
    if (tokens.size() >= kMaxTokens) return false;
    token_entry entry;
    entry.user = session.user;
    entry.created = session.created;
    entry.fingerprint = session.fingerprint;
    // It was exported, so it was held by a client; keep it exportable.
    entry.persistent = true;
    tokens[session.hash] = entry;
    return true;
  }

  void revoke(const std::string &token) {
    const std::string key = key_for(token);
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tokens.find(key);
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
  std::string get_role(const std::string &user) const;
  void add_grant(const std::string &role, const std::string &grant);
};

// Serialising the session table for the core storage (`nsclient.db`). The
// whole table goes into ONE storage row, replaced at every shutdown. The core
// storage has no delete, so one row per session would have left a blank
// tombstone behind for every session that ever expired or logged out - and
// nothing would ever have removed them. One row has nothing to tombstone: an
// empty table is written as an empty value, and a single put replaces
// whatever the previous run left.
//
// The value is a format version on the first line, then one record per line:
//
//     1
//     <hash><TAB><user><TAB><created-epoch><TAB><fingerprint>
//     ...
//
// A later format bumps the version so it is told apart from this one rather
// than silently mis-parsed.
namespace session_persistence {
// The storage row for a session table. A session that cannot be represented
// (a user containing a tab or a newline would make the record ambiguous, a
// hash that is not one) is left out. An empty table serialises to "".
std::string serialize_sessions(const std::list<token_store::persisted_session> &sessions);

// Parse a storage row back into a session table. Returns an empty list for
// an empty or unversioned value; a malformed record is skipped, the rest of
// the table still loads.
std::list<token_store::persisted_session> parse_sessions(const std::string &value);
}  // namespace session_persistence
