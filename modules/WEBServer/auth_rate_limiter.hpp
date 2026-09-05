// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <ctime>
#include <mutex>
#include <string>

// Per-IP failed-auth backoff. Each consecutive failure increases the failure
// count; on reaching `max_failures` the IP is blocked. A successful auth clears
// the counter.
//
// The block escalates: the first block lasts `block_seconds`, and every further
// block from the same IP doubles it up to `kMaxBackoffShift` doublings (and no
// further than `kMaxBlockSeconds` unless `block_seconds` itself is larger). A
// fixed window let an attacker keep spending `max_failures` guesses per window
// forever - ~14k guesses/day per IP at the defaults; doubling turns that into a
// handful per day after a few rounds, at no cost to anyone typing a password
// wrong twice. An IP that goes quiet for `kOffenseDecaySeconds` past the end of
// its block starts over from `block_seconds`, so a shared NAT address does not
// accumulate hour-long blocks over a month of stray typos.
//
// This is intentionally simple: no global cap. The map can grow with distinct
// hostile IPs but each entry is small (one string + two ints and a timestamp)
// and successful traffic prunes its own entry. For protection against
// IP-rotating attackers a separate global rate limit would be needed - out of
// scope here.
//
// Setting `max_failures` to 0 disables the limiter entirely (every call to
// `is_blocked` short-circuits to false). Useful for integration test harnesses
// that intentionally probe auth failures.
class auth_rate_limiter {
 public:
  static constexpr int kDefaultMaxFailures = 10;
  static constexpr int kDefaultBlockSeconds = 60;
  // How many times the block may double before it stops growing.
  static constexpr int kMaxBackoffShift = 6;
  // Ceiling for the escalated block (the defaults reach it: 60 s doubled six
  // times is 64 min, clamped to 60), unless `block_seconds` is already above it.
  static constexpr long kMaxBlockSeconds = 3600;
  // Quiet time after a block expires that resets the escalation.
  static constexpr long kOffenseDecaySeconds = 3600;

  void set_max_failures(int v) { max_failures_ = v; }
  void set_block_seconds(int v) { block_seconds_ = v; }
  int get_max_failures() const { return max_failures_; }
  int get_block_seconds() const { return block_seconds_; }

  bool is_blocked(const std::string& ip) {
    if (max_failures_ <= 0) return false;
    std::lock_guard<std::mutex> g(mu);
    const auto it = entries.find(ip);
    if (it == entries.end()) return false;
    return it->second.blocked_until > std::time(nullptr);
  }

  void record_failure(const std::string& ip) {
    if (max_failures_ <= 0) return;
    std::lock_guard<std::mutex> g(mu);
    const std::time_t now = std::time(nullptr);
    auto& e = entries[ip];
    // Escalation decays: an IP that has been quiet since well after its last
    // block ended is treated as a first offender again.
    if (e.blocked_until != 0 && now > e.blocked_until + kOffenseDecaySeconds) e.offenses = 0;
    e.failures++;
    if (e.failures >= max_failures_) {
      if (e.offenses < kMaxBackoffShift + 1) e.offenses++;
      e.blocked_until = now + block_duration_seconds(block_seconds_, e.offenses);
      e.failures = 0;
    }
  }

  // Duration of the `offenses`-th consecutive block (1-based). Pure and static
  // so the escalation curve can be asserted without waiting out a block.
  static long block_duration_seconds(int base_seconds, int offenses) {
    if (base_seconds <= 0) return 0;
    int doublings = offenses > 0 ? offenses - 1 : 0;
    if (doublings > kMaxBackoffShift) doublings = kMaxBackoffShift;
    // A `block seconds` configured above the ceiling is an explicit operator
    // choice; never shorten it.
    long ceiling = kMaxBlockSeconds;
    if (base_seconds > ceiling) ceiling = base_seconds;
    long delay = base_seconds;
    for (int i = 0; i < doublings && delay < ceiling; ++i) delay *= 2;
    return delay > ceiling ? ceiling : delay;
  }

  void record_success(const std::string& ip) {
    std::lock_guard<std::mutex> g(mu);
    entries.erase(ip);
  }

  // For tests.
  void clear() {
    std::lock_guard<std::mutex> g(mu);
    entries.clear();
  }

  // For tests: when the current block for `ip` expires (0 when never blocked).
  std::time_t blocked_until(const std::string& ip) {
    std::lock_guard<std::mutex> g(mu);
    const auto it = entries.find(ip);
    return it == entries.end() ? 0 : it->second.blocked_until;
  }

 private:
  struct entry {
    int failures = 0;
    // Consecutive blocks handed to this IP; drives the backoff doubling.
    int offenses = 0;
    std::time_t blocked_until = 0;
  };
  boost::unordered_map<std::string, entry> entries;
  std::mutex mu;
  int max_failures_ = kDefaultMaxFailures;
  int block_seconds_ = kDefaultBlockSeconds;
};
