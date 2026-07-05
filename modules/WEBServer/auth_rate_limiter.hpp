// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <ctime>
#include <mutex>
#include <string>

// Per-IP failed-auth backoff. Each consecutive failure increases the failure
// count; on reaching `max_failures` the IP is blocked for `block_seconds`. A
// successful auth clears the counter.
//
// This is intentionally simple: no decay, no global cap. The map can grow with
// distinct hostile IPs but each entry is small (one string + two ints) and
// successful traffic prunes its own entry. For protection against IP-rotating
// attackers a separate global rate limit would be needed - out of scope here.
//
// Setting `max_failures` to 0 disables the limiter entirely (every call to
// `is_blocked` short-circuits to false). Useful for integration test harnesses
// that intentionally probe auth failures.
class auth_rate_limiter {
 public:
  static constexpr int kDefaultMaxFailures = 10;
  static constexpr int kDefaultBlockSeconds = 60;

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
    auto& e = entries[ip];
    e.failures++;
    if (e.failures >= max_failures_) {
      e.blocked_until = std::time(nullptr) + block_seconds_;
      e.failures = 0;
    }
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

 private:
  struct entry {
    int failures = 0;
    std::time_t blocked_until = 0;
  };
  boost::unordered_map<std::string, entry> entries;
  std::mutex mu;
  int max_failures_ = kDefaultMaxFailures;
  int block_seconds_ = kDefaultBlockSeconds;
};
