// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Bounded, in-memory cache of passive check results.
//
// The WEB server registers a submission channel and drops everything that
// arrives on it in here, so that a monitoring system which cannot reach the
// agent (behind NAT, on a laptop, ...) can still be served its results by
// polling /api/v2/results instead of the agent pushing to it.
//
// The store is a *cache*, not a queue: results are keyed, and a new result
// for a key replaces the previous one rather than piling up behind it. That
// is what makes it bounded in normal operation - one entry per monitored
// thing, however often it reports. The cap and the age limit exist for the
// abnormal case (a producer that invents a fresh key every submission).
struct result_store {
  struct result_entry {
    // Identity of the monitored thing. Built from the configured primary
    // index (see result_key_formatter); a second result with the same key
    // overwrites this one.
    std::string key;
    std::string channel;
    // The submitting host as resolved from the request header, and the raw
    // sender id it was resolved from. They differ when the sender did not
    // list itself in the header's host table.
    std::string host;
    std::string source;
    std::string command;
    std::string alias;
    // Nagios status: 0 OK, 1 WARNING, 2 CRITICAL, 3 UNKNOWN.
    int status;
    std::string message;
    std::string perf;

    // Stamped by the store, not by the caller.
    std::size_t index;
    std::size_t count;
    std::int64_t first_seen;
    std::int64_t last_seen;

    result_entry() : status(3), index(0), count(0), first_seen(0), last_seen(0) {}
  };

  typedef std::vector<result_entry> result_list;

  // Every field is optional; an empty field matches everything. Set fields
  // are matched case-insensitively and exactly (not as a substring), so that
  // `?host=srv1` cannot accidentally also return `srv10`.
  struct filter {
    std::string channel;
    std::string host;
    std::string command;
    std::string alias;
    // Empty = any status.
    std::vector<int> statuses;

    bool matches(const result_entry &entry) const;
  };

  static const std::size_t kDefaultMaxEntries = 1000;

  result_store() : next_index_(0), max_entries_(kDefaultMaxEntries), max_age_(0) {}

  // Store a result, replacing any previous result carrying the same key.
  // `now` is injectable so the tests do not have to sleep.
  void submit(const result_entry &entry, std::int64_t now);
  void submit(const result_entry &entry);

  // Sorted by key so that a client paging through the list sees a stable
  // order even while results keep arriving.
  result_list list(const filter &f, std::int64_t now) const;
  result_list list() const;

  bool get(const std::string &key, result_entry &out, std::int64_t now) const;
  bool get(const std::string &key, result_entry &out) const;

  // Returns false when there was nothing to remove.
  bool remove(const std::string &key);
  // Number of entries dropped.
  std::size_t clear();

  std::size_t size() const;

  // 0 is clamped to 1: "cache nothing" is a silently useless configuration.
  void set_max_entries(std::size_t value);
  // Seconds; 0 disables expiry. Expired entries are dropped lazily on the
  // next read or write, so nothing has to run a timer.
  void set_max_age(std::int64_t seconds);

 private:
  typedef std::map<std::string, result_entry> entry_map;

  // Callers must hold the lock.
  void expire(std::int64_t now);
  void trim();

  mutable boost::timed_mutex mutex_;
  entry_map entries_;
  std::size_t next_index_;
  std::size_t max_entries_;
  std::int64_t max_age_;
};

// Seconds since the unix epoch - the clock the store stamps results with.
std::int64_t result_store_now();

// Expands the configured primary-index expression (`${host}/${alias-or-command}`
// and friends) into the key a result is cached under. Kept separate from the
// store so the store stays a plain container.
struct result_key_formatter {
  // Starts out formatting kDefaultExpression, so a formatter that was never
  // handed a configured expression still produces usable keys.
  result_key_formatter();

  // Returns false (and falls back to the default expression) when the
  // expression references an unknown variable; `error` then says which.
  bool parse(const std::string &expression, std::string &error);
  std::string format(const result_store::result_entry &entry) const;

  static const char *kDefaultExpression;

 private:
  struct token {
    // Literal text when `variable` is empty.
    std::string text;
    std::string variable;
  };
  std::vector<token> tokens_;
};
