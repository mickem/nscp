#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <ctime>
#include <mutex>
#include <string>

// Per-IP failed-auth backoff. Each consecutive failure increases the failure
// count; on reaching kMaxFailures the IP is blocked for kBlockSeconds. A
// successful auth clears the counter.
//
// This is intentionally simple: no decay, no global cap. The map can grow with
// distinct hostile IPs but each entry is small (one string + two ints) and
// successful traffic prunes its own entry. For protection against IP-rotating
// attackers a separate global rate limit would be needed - out of scope here.
class auth_rate_limiter {
 public:
  static constexpr int kMaxFailures = 10;
  static constexpr int kBlockSeconds = 60;

  bool is_blocked(const std::string& ip) {
    std::lock_guard<std::mutex> g(mu);
    const auto it = entries.find(ip);
    if (it == entries.end()) return false;
    return it->second.blocked_until > std::time(nullptr);
  }

  void record_failure(const std::string& ip) {
    std::lock_guard<std::mutex> g(mu);
    auto& e = entries[ip];
    e.failures++;
    if (e.failures >= kMaxFailures) {
      e.blocked_until = std::time(nullptr) + kBlockSeconds;
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
};
