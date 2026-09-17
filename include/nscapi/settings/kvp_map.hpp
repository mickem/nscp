// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Lightweight container for [/settings/.../<section>] sections that hold a
// flat `name = value` map. Use this when each entry is a leaf definition and
// you don't need the per-entry subdirectory, parent / is_template / alias
// keys that object_handler provides.
//
// Typical wiring in loadModuleEx:
//
//   nscapi::settings::kvp_map<MyValue> entries(
//       [](const std::string& name, const std::string& raw) {
//         return parse_my_value(name, raw);  // returns boost::optional<MyValue>
//       });
//   entries.set_path(settings.alias().get_settings_path("things"));
//   settings.alias().add_path_to_settings()
//     ("things", sh::fun_values_path(
//         [&entries](auto k, auto v) { entries.add(k, v); }),
//       "Title", "Description");
//
// Then entries.find(name) / entries.for_each(...) at runtime.

#pragma once

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/optional.hpp>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace nscapi {
namespace settings {

template <class Value>
class kvp_map {
 public:
  // Convert the raw on-disk value to a domain object. Return boost::none to
  // reject (the caller is expected to have already logged); the entry will
  // not be inserted.
  using parse_fn = std::function<boost::optional<Value>(const std::string &name, const std::string &raw)>;

  // case_insensitive=true lowercases the key both at insert time and at
  // lookup time. Use it when the entry name comes off the wire (e.g. an
  // incoming command name) where casing varies. Leave it false for
  // operator-facing names where casing is meaningful and stable.
  explicit kvp_map(parse_fn parser, bool case_insensitive = false) : parser_(std::move(parser)), case_insensitive_(case_insensitive) {}

  void set_path(const std::string &path) { path_ = path; }
  const std::string &get_path() const { return path_; }

  void add(const std::string &name, const std::string &raw) {
    if (name.empty()) return;
    const std::string key = normalize(name);
    // Outside the lock: the parser logs, and logging can reach back into the
    // module that owns this map.
    boost::optional<Value> parsed = parser_(key, raw);
    if (!parsed) return;
    std::lock_guard<std::mutex> lock(mutex_);
    entries_[key] = std::move(parsed.value());
  }

  // Take over `other`'s entries, in one step.
  //
  // A settings reload runs on the scheduler thread while queries are resolving
  // names against this map. Adding into the live map re-balanced the tree under
  // a reader, and because nothing ever removed an entry an alias deleted from
  // the ini went on resolving until the agent was restarted. Build the
  // replacement in a local and hand it over here: a lookup sees either the
  // whole old table or the whole new one, and what is gone is gone.
  void replace(kvp_map &other) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.swap(other.entries_);
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
  }

  boost::optional<Value> find(const std::string &name) const {
    const std::string key = normalize(name);
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = entries_.find(key);
    if (it == entries_.end()) return boost::none;
    return it->second;
  }

  bool contains(const std::string &name) const {
    const std::string key = normalize(name);
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.find(key) != entries_.end();
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.empty();
  }
  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
  }

  // Walks a copy: `fn` is caller code that may query, log or add, and holding
  // the lock across it would deadlock the moment it comes back here.
  template <class F>
  void for_each(F &&fn) const {
    std::map<std::string, Value> snapshot;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      snapshot = entries_;
    }
    for (const auto &kv : snapshot) fn(kv.first, kv.second);
  }

 private:
  std::string normalize(const std::string &name) const { return case_insensitive_ ? boost::algorithm::to_lower_copy(name) : name; }

  std::string path_;
  parse_fn parser_;
  bool case_insensitive_;
  // Guards entries_ only. Every accessor takes it: the settings callbacks run
  // on the reload thread and the lookups on whatever thread serves the query.
  mutable std::mutex mutex_;
  std::map<std::string, Value> entries_;
};

// Convenience factory for the common kvp_map<std::string> case (identity parse,
// case-sensitive). Saves callers from spelling out a passthrough lambda.
inline kvp_map<std::string> make_string_kvp_map(bool case_insensitive = false) {
  return kvp_map<std::string>([](const std::string &, const std::string &raw) -> boost::optional<std::string> { return raw; }, case_insensitive);
}

}  // namespace settings
}  // namespace nscapi
