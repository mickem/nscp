// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "result_store.hpp"

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <parsers/expression/expression.hpp>
#include <utility>

namespace {
// Every read and write takes the same bounded wait as the event store: a
// wedged HTTP worker must not take the submission path down with it (or the
// other way round).
boost::posix_time::ptime lock_deadline() { return boost::get_system_time() + boost::posix_time::seconds(5); }

bool iequals(const std::string &lhs, const std::string &rhs) { return boost::algorithm::iequals(lhs, rhs); }
}  // namespace

std::int64_t result_store_now() {
  static const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  return (boost::posix_time::second_clock::universal_time() - epoch).total_seconds();
}

const std::size_t result_store::kDefaultMaxEntries;

bool result_store::filter::matches(const result_entry &entry) const {
  if (!channel.empty() && !iequals(channel, entry.channel)) return false;
  if (!host.empty() && !iequals(host, entry.host)) return false;
  if (!command.empty() && !iequals(command, entry.command)) return false;
  if (!alias.empty() && !iequals(alias, entry.alias)) return false;
  if (!statuses.empty() && std::find(statuses.begin(), statuses.end(), entry.status) == statuses.end()) return false;
  return true;
}

void result_store::submit(const result_entry &entry, const std::int64_t now) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return;
  if (!enabled_) return;

  expire(now);

  const entry_map::iterator it = entries_.find(entry.key);
  if (it == entries_.end()) {
    result_entry stored = entry;
    stored.first_seen = now;
    stored.count = 1;
    stored.last_seen = now;
    stored.result_seen = now;
    // Bumped on every submission, so the lowest index is always the least
    // recently updated entry - which is the one trim() evicts.
    stored.index = next_index_++;
    entries_[stored.key] = std::move(stored);
    trim();
    return;
  }

  result_entry &current = it->second;
  // The key's history survives whichever result wins: when it was first seen
  // and how many times it has reported. That is what turns the cache into
  // something a monitoring system can reason about rather than a bare
  // last-value register.
  const std::int64_t first_seen = current.first_seen;
  const std::size_t count = current.count + 1;

  // In `worst` mode a less severe result is recorded as a submission but does
  // not get to overwrite the problem it is recovering from - that is the
  // point: a CRITICAL that recovers between two polls must still be seen.
  // Equal severity does replace, so the message stays as current as its
  // severity allows.
  const bool keep_current = mode_ == mode_worst && entry.status < current.status;
  if (!keep_current) {
    current = entry;  // same key by construction - this is the map slot it was found in
    current.result_seen = now;
  }
  current.first_seen = first_seen;
  current.count = count;
  current.last_seen = now;
  current.index = next_index_++;
}

void result_store::submit(const result_entry &entry) { submit(entry, result_store_now()); }

result_store::result_list result_store::list(const filter &f, const std::int64_t now) const {
  result_list ret;
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return ret;

  for (const entry_map::value_type &kv : entries_) {
    if (max_age_ > 0 && (now - kv.second.last_seen) > max_age_) continue;
    if (!f.matches(kv.second)) continue;
    ret.push_back(kv.second);
  }
  return ret;
}

result_store::result_list result_store::list() const { return list(filter(), result_store_now()); }

result_store::result_list result_store::drain(const filter &f, const std::int64_t now) {
  result_list ret;
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return ret;

  for (entry_map::iterator it = entries_.begin(); it != entries_.end();) {
    const bool expired = max_age_ > 0 && (now - it->second.last_seen) > max_age_;
    if (expired) {
      // On its way out anyway, and the caller never saw it.
      it = entries_.erase(it);
      continue;
    }
    if (!f.matches(it->second)) {
      ++it;
      continue;
    }
    ret.push_back(it->second);
    it = entries_.erase(it);
  }
  return ret;
}

bool result_store::get(const std::string &key, result_entry &out, const std::int64_t now) const {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return false;

  const entry_map::const_iterator it = entries_.find(key);
  if (it == entries_.end()) return false;
  if (max_age_ > 0 && (now - it->second.last_seen) > max_age_) return false;
  out = it->second;
  return true;
}

bool result_store::get(const std::string &key, result_entry &out) const { return get(key, out, result_store_now()); }

bool result_store::remove(const std::string &key) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return false;
  return entries_.erase(key) > 0;
}

std::size_t result_store::clear() {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return 0;
  const std::size_t dropped = entries_.size();
  entries_.clear();
  return dropped;
}

std::size_t result_store::size() const {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return 0;
  if (max_age_ <= 0) return entries_.size();
  const std::int64_t now = result_store_now();
  std::size_t count = 0;
  for (const entry_map::value_type &kv : entries_) {
    if ((now - kv.second.last_seen) <= max_age_) count++;
  }
  return count;
}

void result_store::set_enabled(const bool value) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return;
  // Turning the cache off empties it: leaving results behind would keep
  // serving them from an endpoint the operator has just switched off.
  if (!value) entries_.clear();
  enabled_ = value;
}

bool result_store::is_enabled() const {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return false;
  return enabled_;
}

void result_store::set_mode(const cache_mode value) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return;
  mode_ = value;
}

result_store::cache_mode result_store::mode() const {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return mode_last;
  return mode_;
}

bool result_store::parse_mode(const std::string &value, cache_mode &out) {
  std::string name = value;
  boost::algorithm::trim(name);
  boost::algorithm::to_lower(name);
  if (name == "last") {
    out = mode_last;
    return true;
  }
  if (name == "worst") {
    out = mode_worst;
    return true;
  }
  return false;
}

const char *result_store::mode_name(const cache_mode value) { return value == mode_worst ? "worst" : "last"; }

void result_store::set_max_entries(const std::size_t value) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return;
  max_entries_ = value == 0 ? 1 : value;
  trim();
}

void result_store::set_max_age(const std::int64_t seconds) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, lock_deadline());
  if (!lock.owns_lock()) return;
  max_age_ = seconds < 0 ? 0 : seconds;
}

void result_store::expire(const std::int64_t now) {
  if (max_age_ <= 0) return;
  for (entry_map::iterator it = entries_.begin(); it != entries_.end();) {
    if ((now - it->second.last_seen) > max_age_) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

void result_store::trim() {
  while (entries_.size() > max_entries_) {
    entry_map::iterator oldest = entries_.begin();
    for (entry_map::iterator it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->second.index < oldest->second.index) oldest = it;
    }
    entries_.erase(oldest);
  }
}

const char *result_key_formatter::kDefaultExpression = "${host}/${alias-or-command}";

result_key_formatter::result_key_formatter() {
  std::string ignored;
  parse(kDefaultExpression, ignored);
}

bool result_key_formatter::parse(const std::string &expression, std::string &error) {
  const std::string source = expression.empty() ? std::string(kDefaultExpression) : expression;

  parsers::simple_expression::result_type parsed;
  std::vector<token> tokens;
  if (parsers::simple_expression::parse(source, parsed)) {
    for (const parsers::simple_expression::entry &e : parsed) {
      token t;
      if (!e.is_variable) {
        t.text = e.name;
      } else if (e.name == "host" || e.name == "source" || e.name == "channel" || e.name == "command" || e.name == "alias" || e.name == "alias-or-command") {
        t.variable = e.name;
      } else {
        error = "Invalid primary index variable: " + e.name;
        tokens.clear();
        break;
      }
      tokens.push_back(t);
    }
  } else {
    error = "Failed to parse primary index: " + source;
  }

  if (!error.empty()) {
    // Fall back to something usable rather than caching everything under an
    // empty key, which would collapse every result into one entry.
    tokens.clear();
    parsers::simple_expression::result_type fallback;
    parsers::simple_expression::parse(kDefaultExpression, fallback);
    for (const parsers::simple_expression::entry &e : fallback) {
      token t;
      if (e.is_variable) {
        t.variable = e.name;
      } else {
        t.text = e.name;
      }
      tokens.push_back(t);
    }
    tokens_.swap(tokens);
    return false;
  }
  tokens_.swap(tokens);
  return true;
}

std::string result_key_formatter::format(const result_store::result_entry &entry) const {
  std::string key;
  for (const token &t : tokens_) {
    if (t.variable.empty()) {
      key += t.text;
    } else if (t.variable == "host") {
      key += entry.host;
    } else if (t.variable == "source") {
      key += entry.source;
    } else if (t.variable == "channel") {
      key += entry.channel;
    } else if (t.variable == "command") {
      key += entry.command;
    } else if (t.variable == "alias") {
      key += entry.alias;
    } else if (t.variable == "alias-or-command") {
      key += entry.alias.empty() ? entry.command : entry.alias;
    }
  }
  return key;
}
