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
// The block escalates, but only against a machine-speed guesser. A fixed window
// let an attacker keep spending `max_failures` guesses per window forever -
// ~14k guesses/day per IP at the defaults - so a block whose failures arrived
// faster than one every `kBurstGapSeconds` on average doubles the next block,
// up to `kMaxBackoffShift` doublings and never past `kMaxBlockSeconds`.
//
// A run of failures slower than that keeps the base `block_seconds` no matter
// how long it goes on. That is deliberate, and it is the only thing standing
// between one broken client and everyone who shares its address: the limiter
// keys on the socket peer, so behind NAT or a reverse proxy a single monitoring
// client retrying a stale password every few seconds would otherwise ratchet
// the shared address to the ceiling and hold it there, locking every other
// client out of logging in. A client slow enough to keep the base delay is by
// then guessing so slowly that the base block already bounds it - the old,
// unescalated behaviour, which is the worst this can do to a bystander.
//
// The escalation also decays: an IP that goes quiet for `kOffenseDecaySeconds`
// past the end of its block starts over from `block_seconds`.
//
// This is intentionally simple: no global cap. The map can grow with distinct
// hostile IPs but each entry is small (one string, two ints and two timestamps)
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
  // Absolute ceiling for a block, in seconds (one hour). The defaults reach it:
  // 60 s doubled six times is 64 min, clamped back to this 60 min. A
  // `block seconds` configured at or above the ceiling is an explicit operator
  // choice: it is never shortened, and it never escalates either - it is
  // already longer than any escalated block would be.
  static constexpr long kMaxBlockSeconds = 3600;
  // Quiet time after a block expires that resets the escalation.
  static constexpr long kOffenseDecaySeconds = 3600;
  // Average seconds between the failures of one run, below which the run counts
  // as automated and escalates the next block. A human at a login form, a
  // monitoring poller with a stale password and a browser retrying are all far
  // slower than this.
  static constexpr long kBurstGapSeconds = 2;

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

  void record_failure(const std::string& ip) { record_failure_at(ip, std::time(nullptr)); }

  // record_failure with an explicit clock, so a test can drive the burst
  // window, the escalation and the decay without waiting any of them out.
  void record_failure_at(const std::string& ip, std::time_t now) {
    if (max_failures_ <= 0) return;
    std::lock_guard<std::mutex> g(mu);
    auto& e = entries[ip];
    // An IP that has been quiet since well after its last block ended is
    // treated as a first offender again.
    if (e.blocked_until != 0 && now > e.blocked_until + kOffenseDecaySeconds) e.offenses = 0;
    if (e.failures == 0) e.first_failure = now;
    e.failures++;
    if (e.failures >= max_failures_) {
      if (is_burst(now - e.first_failure)) {
        if (e.offenses < kMaxBackoffShift + 1) e.offenses++;
      } else {
        // Too slow to be a guessing loop: back to the base delay, however many
        // rounds this client has already been through.
        e.offenses = 1;
      }
      e.blocked_until = now + block_duration_seconds(block_seconds_, e.offenses);
      e.failures = 0;
      e.first_failure = 0;
    }
  }

  // Whether a run of `max_failures` failures spanning `elapsed` seconds came in
  // fast enough to count as automated. Free of any per-IP state, so the
  // threshold can be asserted directly.
  bool is_burst(std::time_t elapsed) const {
    const long gaps = max_failures_ > 1 ? max_failures_ - 1 : 1;
    return elapsed < gaps * kBurstGapSeconds;
  }

  // Duration of the `offenses`-th consecutive block (1-based). Pure and static
  // so the escalation curve can be asserted without waiting out a block.
  static long block_duration_seconds(int base_seconds, int offenses) {
    if (base_seconds <= 0) return 0;
    // A base at or above the ceiling already exceeds every escalated block, so
    // there is nothing to escalate into: hand back what the operator asked for.
    if (base_seconds >= kMaxBlockSeconds) return base_seconds;
    int doublings = offenses > 0 ? offenses - 1 : 0;
    if (doublings > kMaxBackoffShift) doublings = kMaxBackoffShift;
    long delay = base_seconds;
    for (int i = 0; i < doublings && delay < kMaxBlockSeconds; ++i) delay *= 2;
    return delay > kMaxBlockSeconds ? kMaxBlockSeconds : delay;
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
    // When the current run of failures started; decides whether that run was
    // fast enough to escalate.
    std::time_t first_failure = 0;
    std::time_t blocked_until = 0;
  };
  boost::unordered_map<std::string, entry> entries;
  std::mutex mu;
  int max_failures_ = kDefaultMaxFailures;
  int block_seconds_ = kDefaultBlockSeconds;
};
