// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>

namespace net {

// Decides which refused connections are worth a log line. A listener that logs
// every refusal hands any host that can reach the port a way to flood the log,
// and the line is written on the thread that accepts connections. This lets
// the first refusal from an address through, then at most one per `window`,
// carrying how many were left out in between.
//
// The table is bounded: once `max_addresses` are tracked (after dropping the
// ones whose window has passed), further addresses share one entry, so a
// caller cycling through addresses gets one line per window between them.
class connection_log_throttle {
 public:
  typedef std::chrono::steady_clock clock;

  explicit connection_log_throttle(const std::chrono::seconds window = std::chrono::seconds(60), const std::size_t max_addresses = 1024)
      : window_(window), max_addresses_(max_addresses) {}

  // Whether to log this refusal of `address`. When true, `suppressed` is how
  // many refusals went unlogged since the last line for it (or, when the
  // table is full, for the addresses sharing the overflow entry).
  bool should_log(const std::string &address, std::size_t &suppressed) { return should_log_at(address, suppressed, clock::now()); }

  // should_log() with an explicit clock, for tests.
  bool should_log_at(const std::string &address, std::size_t &suppressed, const clock::time_point now) {
    const std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(address);
    if (it == entries_.end()) {
      if (entries_.size() >= max_addresses_) prune(now);
      // The empty key is the shared overflow entry: no peer address is empty.
      const std::string key = entries_.size() >= max_addresses_ ? std::string() : address;
      it = entries_.find(key);
      if (it == entries_.end()) {
        entries_[key] = entry{now, 0};
        suppressed = 0;
        return true;
      }
    }
    entry &e = it->second;
    if (now - e.last_logged < window_) {
      e.suppressed++;
      return false;
    }
    suppressed = e.suppressed;
    e.suppressed = 0;
    e.last_logged = now;
    return true;
  }

  std::size_t tracked() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
  }

 private:
  struct entry {
    clock::time_point last_logged;
    std::size_t suppressed;
  };

  // Drops the addresses that could log again anyway and have nothing pending.
  void prune(const clock::time_point now) {
    for (auto it = entries_.begin(); it != entries_.end();) {
      if (now - it->second.last_logged >= window_ && it->second.suppressed == 0) {
        it = entries_.erase(it);
      } else {
        ++it;
      }
    }
  }

  const std::chrono::seconds window_;
  const std::size_t max_addresses_;
  std::map<std::string, entry> entries_;
  mutable std::mutex mutex_;
};

}  // namespace net
