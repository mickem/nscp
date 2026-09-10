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
// The store keeps exactly one entry per key. When a second result arrives for
// a key the `mode` decides which of the two survives: the newer one (`last`),
// or the more severe one (`worst`). Paired with a draining poll, `worst` is
// what stops a CRITICAL that recovered between two polls from going unseen,
// which is the whole reason the mode exists.
//
// The store is therefore bounded in normal operation - one entry per
// monitored thing, however often it reports. The entry cap and the age limit
// exist for the abnormal case: a producer that invents a fresh key every
// submission.
struct result_store {
  // Which of two results for the same key is kept. The ordering is a plain
  // numeric max over the Nagios status, matching how the rest of NSClient++
  // aggregates a worst-of (see parse_simple_exec_response), so UNKNOWN (3)
  // ranks above CRITICAL (2).
  enum cache_mode {
    // The newest result wins. A recovery replaces the problem that preceded it.
    mode_last,
    // The most severe result wins; an equally severe one still replaces it, so
    // the message stays as current as its severity allows.
    mode_worst
  };

  struct result_entry {
    // Identity of the monitored thing. Built from the configured primary
    // index (see result_key_formatter); a second result with the same key
    // resolves against this one per the cache mode.
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
    // When the key last submitted anything, whether or not that submission
    // won. This is what `age` and the expiry are measured from: it answers
    // "is this check still reporting".
    std::int64_t last_seen;
    // When the result actually being served arrived. Equal to last_seen in
    // `last` mode; in `worst` mode it is when the retained problem happened,
    // which a consumer needs in order to tell a fresh CRITICAL from one that
    // has been held since the last poll.
    std::int64_t result_seen;

    result_entry() : status(3), index(0), count(0), first_seen(0), last_seen(0), result_seen(0) {}
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

  result_store() : enabled_(false), mode_(mode_last), clear_on_poll_(true), next_index_(0), max_entries_(kDefaultMaxEntries), max_age_(0) {}

  // Store a result, resolving a collision on the key per the cache mode.
  // Returns false when the result was NOT stored: the cache is disabled, or
  // the lock was not acquired within the deadline. A submitter has to be able
  // to tell the difference, or it reports a result as cached that was
  // dropped. `now` is injectable so the tests do not have to sleep.
  bool submit(const result_entry &entry, std::int64_t now);
  bool submit(const result_entry &entry);

  // Sorted by key so that a client paging through the list sees a stable
  // order even while results keep arriving.
  result_list list(const filter &f, std::int64_t now) const;
  result_list list() const;

  // list(), but the entries handed back are removed. Only what is returned is
  // dropped, so a filtered poll cannot silently discard results the caller
  // never saw.
  result_list drain(const filter &f, std::int64_t now);

  bool get(const std::string &key, result_entry &out, std::int64_t now) const;
  bool get(const std::string &key, result_entry &out) const;

  // Returns false when there was nothing to remove. An entry past `max age`
  // counts as nothing: it is erased, but reported as absent, so a DELETE and
  // a GET of the same expired key agree.
  bool remove(const std::string &key, std::int64_t now);
  bool remove(const std::string &key);
  // Number of entries dropped.
  std::size_t clear();

  std::size_t size() const;

  // Off by default: until an operator turns the cache on, submissions are
  // dropped and nothing accumulates. The channel is not registered either,
  // so in practice nothing is submitted in the first place - this is the
  // belt to that pair of braces.
  void set_enabled(bool value);
  bool is_enabled() const;

  void set_mode(cache_mode value);
  cache_mode mode() const;

  // Whether a poll consumes what it reports (see drain()). Held here rather
  // than in the controller so that a settings reload reaches it: the
  // controllers are built once, at startup.
  void set_clear_on_poll(bool value);
  bool clear_on_poll() const;

  // 0 is clamped to 1: "cache nothing" is a silently useless configuration.
  void set_max_entries(std::size_t value);
  // Seconds; 0 disables expiry. Nothing runs a timer: an expired entry is
  // hidden from every read as soon as it is too old, and physically erased
  // the next time a submission or a draining poll walks the map. A store
  // that is only ever listed therefore keeps expired entries in memory until
  // the entry cap evicts them - which is what the cap is for.
  void set_max_age(std::int64_t seconds);

  // "last"/"worst", case-insensitively and ignoring surrounding space.
  // Returns false on anything else, leaving `out` untouched.
  static bool parse_mode(const std::string &value, cache_mode &out);
  static const char *mode_name(cache_mode value);

 private:
  typedef std::map<std::string, result_entry> entry_map;

  // Callers must hold the lock.
  void expire(std::int64_t now);
  void trim();

  mutable boost::timed_mutex mutex_;
  entry_map entries_;
  bool enabled_;
  cache_mode mode_;
  bool clear_on_poll_;
  std::size_t next_index_;
  std::size_t max_entries_;
  std::int64_t max_age_;
};

// Seconds since the unix epoch - the clock the store stamps results with.
std::int64_t result_store_now();

// A key is operator-defined text (an alias, a hostname) that travels in a URL
// path: `/api/v2/results/{key}`. Anything outside the unreserved set is
// percent-encoded on the way out and decoded on the way back in, so a key
// holding a space or a `%` still round-trips. `/` is deliberately left alone:
// the default primary index puts one between host and check, and every route
// that captures a key spans path segments.
std::string result_key_encode(const std::string &key);
std::string result_key_decode(const std::string &encoded);

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
