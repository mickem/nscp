// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <chrono>
#include <cstddef>
#include <map>
#include <nscapi/protobuf/facts.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "fact_repository.hpp"

namespace nsclient {
namespace core {

// The on-demand fact sets: which producer answers for which path, and the
// short cache of what they last answered.
//
// A scheduled set (fact_repository) is collected on a round, kept in the
// document, hashed into its revision and uploaded to a fleet server. That is
// the wrong shape for a listing that is large or moves under the agent - a
// performance counter catalog is thousands of entries, and the scripts folder
// changes whenever someone uploads one. An on-demand set is collected when a
// consumer reads its path, kept here for a few seconds so a client cannot
// drive collection in a loop, and never enters the document: nothing here is
// hashed, counted against [/settings/facts] max size, or uploaded.
//
// A producer declares what it has (FactSet.on_demand, on any round or on the
// `declare` probe). A declared id is a set (`counters`) or one key below a set
// (`scripts.python`); the second form is what lets three script modules each
// own one key of `scripts`, where a stored set has exactly one owner. Reading
// `scripts` then asks all three and answers with one object.
//
// This class does no I/O and calls no module: resolve() says who to ask,
// extract() reads what a module answered, assemble() builds the reply. The
// plugin manager does the asking (plugin_manager::read_on_demand), which is
// what makes all of this testable without a module.
//
// Thread safety as fact_repository: one mutex, copies out.
class on_demand_facts {
 public:
  typedef std::chrono::steady_clock clock;

  // How long an answer is served from the cache. Long enough that a page
  // which reads the same set from three widgets collects once, short enough
  // that nobody has to wonder why a script they just added is not listed.
  static constexpr std::chrono::seconds cache_ttl = std::chrono::seconds(5);
  // Cached answers. A parameterized set has one per parameter, so the cache
  // is bounded rather than trusted to stay small.
  static constexpr std::size_t max_cache_entries = 32;
  // A parameter is remote input; this is as long as any object or instance
  // name a producer could plausibly match it against.
  static constexpr std::size_t max_parameter_length = 256;
  // The encoded size of one on-demand answer. It is not stored, so it does
  // not come out of the document's budget - but it is still built in memory
  // and rendered to JSON for whoever asked.
  static constexpr std::size_t max_answer_size = 8 * 1024 * 1024;

  // One producer to ask, and what to do with its answer.
  struct target {
    std::string id;  // the declared id
    unsigned int plugin_id = 0;
    std::string parameter;  // for a parameterized set
    // What is left of the path below the declared id: walked in the answer.
    std::vector<std::string> remainder;
  };

  // What a path resolves to.
  struct plan {
    // The path's first segment is an on-demand set at all. False means the
    // path is the document's business.
    bool on_demand = false;
    // Why an on-demand path cannot be answered (a missing or malformed
    // parameter), and the declared id that is about. Empty otherwise.
    std::string error;
    std::string error_id;
    // The path is a set whose keys are declared separately: the answer is one
    // object with a field per target.
    bool merge = false;
    std::vector<target> targets;
  };

  // What one producer answered for one declared id.
  struct answer {
    bool found = false;
    PB::Facts::Value value;
    std::string gathered;
    std::string error;
  };

  // Replace what one producer declares. Returns what was refused and why, for
  // the caller to log: an id that is not `set` or `set.key`, one another
  // module already declared, or one that would make a set both a single
  // on-demand set and a parent of others.
  std::vector<std::string> declare(const unsigned int plugin_id, const std::map<std::string, bool> &ids) {
    std::vector<std::string> refused;
    boost::unique_lock<boost::mutex> lock(mutex_);
    seen_.insert(plugin_id);
    for (std::map<std::string, entry>::iterator it = declared_.begin(); it != declared_.end();) {
      if (it->second.plugin_id == plugin_id && ids.find(it->first) == ids.end()) {
        drop_cached_locked(it->first);
        it = declared_.erase(it);
      } else {
        ++it;
      }
    }
    for (const std::pair<const std::string, bool> &id : ids) {
      const std::vector<std::string> segments = split(id.first);
      if (segments.empty() || segments.size() > 2 || !valid_segments(segments)) {
        refused.push_back("'" + clip(id.first) + "' is not a valid on-demand fact set id (a set, or one key below a set)");
        continue;
      }
      const std::map<std::string, entry>::const_iterator existing = declared_.find(id.first);
      if (existing != declared_.end() && existing->second.plugin_id != plugin_id) {
        refused.push_back("on-demand fact set '" + id.first + "' is already produced by another module");
        continue;
      }
      if (shape_conflict_locked(segments, plugin_id)) {
        refused.push_back("on-demand fact set '" + id.first + "' conflicts with another on-demand set of '" + segments[0] + "'");
        continue;
      }
      entry e;
      e.plugin_id = plugin_id;
      e.parameterized = id.second;
      declared_[id.first] = e;
    }
    return refused;
  }

  // A module was unloaded: it answers for nothing any more.
  void forget(const unsigned int plugin_id) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    seen_.erase(plugin_id);
    for (std::map<std::string, entry>::iterator it = declared_.begin(); it != declared_.end();) {
      if (it->second.plugin_id == plugin_id) {
        drop_cached_locked(it->first);
        it = declared_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Whether this producer has said what it declares, on a round or on the
  // probe. One that has not - a module loaded since the last round - is
  // probed before a path is resolved.
  bool has_seen(const unsigned int plugin_id) const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return seen_.count(plugin_id) > 0;
  }

  // The declared ids, sorted, with whether each takes a parameter.
  std::vector<std::pair<std::string, bool>> get_declared() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    std::vector<std::pair<std::string, bool>> out;
    for (const std::pair<const std::string, entry> &d : declared_) out.emplace_back(d.first, d.second.parameterized);
    return out;
  }

  // Whether a first path segment is an on-demand set.
  bool covers(const std::string &head) const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return covers_locked(head);
  }

  // Who answers for `path`.
  plan resolve(const std::string &path) const {
    plan result;
    const std::size_t dot = path.find('.');
    const std::string head = path.substr(0, dot);
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (head.empty() || !covers_locked(head)) return result;
    result.on_demand = true;

    // The set itself was declared: one producer, and the rest of the path is
    // its parameter or a walk into its answer.
    const std::map<std::string, entry>::const_iterator whole = declared_.find(head);
    if (whole != declared_.end()) {
      add_target_locked(result, whole, dot == std::string::npos ? std::string() : path.substr(dot + 1), dot != std::string::npos);
      return result;
    }
    // The set's keys were declared one by one. Asked for the set, that is
    // every key that needs no parameter, as one object.
    if (dot == std::string::npos) {
      result.merge = true;
      const std::string prefix = head + ".";
      for (std::map<std::string, entry>::const_iterator it = declared_.lower_bound(prefix); it != declared_.end(); ++it) {
        if (it->first.compare(0, prefix.size(), prefix) != 0) break;
        if (it->second.parameterized) continue;
        target t;
        t.id = it->first;
        t.plugin_id = it->second.plugin_id;
        result.targets.push_back(t);
      }
      return result;
    }
    // Asked for one key, or below it.
    const std::size_t second = path.find('.', dot + 1);
    const std::map<std::string, entry>::const_iterator key = declared_.find(path.substr(0, second));
    if (key == declared_.end()) return result;  // on demand, but nobody produces this key
    add_target_locked(result, key, second == std::string::npos ? std::string() : path.substr(second + 1), second != std::string::npos);
    return result;
  }

  // Read one declared id out of what its producer answered.
  //
  // The producer builds the set the way it builds any other
  // (`out.set("scripts").object("python")`), so the answer is the FactSet
  // named after the id's first segment, and for a `set.key` id the value of
  // that key inside it. Checked against the document rules before it is
  // served: an on-demand set is the same shape a stored one is.
  static answer extract(const std::string &response, const std::string &id) {
    answer result;
    const std::size_t dot = id.find('.');
    const std::string head = id.substr(0, dot);
    if (response.empty()) {
      result.error = "returned no facts document";
      return result;
    }
    PB::Facts::FactsMessage message;
    if (!message.ParseFromString(response) || message.payload_size() == 0) {
      result.error = "returned facts that are not a valid facts message";
      return result;
    }
    const PB::Facts::FactsMessage::Response &payload = message.payload(0);
    if (payload.result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
      result.error = payload.result().message().empty() ? "failed to collect" : payload.result().message();
      return result;
    }
    for (const PB::Facts::FactSet &set : payload.sets()) {
      // A declaration riding along is not an answer.
      if (set.on_demand() && !set.has_facts()) continue;
      if (set.id() != head && set.id() != id) continue;
      if (!set.error().empty()) result.error = set.error();
      if (!set.gathered().empty()) result.gathered = set.gathered();
      if (set.id() != head || !set.has_facts() || result.found) continue;

      std::string problem;
      if (!fact_repository::validate_set(head, set.facts(), problem)) {
        result.error = problem;
        continue;
      }
      if (set.facts().ByteSizeLong() > max_answer_size) {
        result.error = "on-demand fact set '" + id + "' is larger than the " + std::to_string(max_answer_size) + " bytes an answer may be";
        continue;
      }
      PB::Facts::Object sorted = set.facts();
      nscapi::facts::tree::sort_fields(&sorted);
      if (dot == std::string::npos) {
        *result.value.mutable_object_value() = sorted;
        result.found = true;
        continue;
      }
      const PB::Facts::Value *child = nscapi::facts::tree::get(sorted, id.substr(dot + 1));
      if (child == nullptr) continue;
      const PB::Facts::Value copy = *child;
      result.value = copy;
      result.found = true;
    }
    return result;
  }

  // The reply to a path: the one answer (walked down to what was asked for),
  // or one object holding every key that was found. False when nothing
  // produced what was asked.
  static bool assemble(const plan &resolved, const std::vector<answer> &answers, PB::Facts::Value &out) {
    if (resolved.merge) {
      // The targets come out of a sorted map, so the fields are in the
      // document's canonical order without sorting again.
      bool any = false;
      PB::Facts::Object *object = out.mutable_object_value();
      for (std::size_t i = 0; i < resolved.targets.size() && i < answers.size(); ++i) {
        if (!answers[i].found) continue;
        PB::Facts::Field *field = object->add_fields();
        field->set_key(resolved.targets[i].id.substr(resolved.targets[i].id.find('.') + 1));
        *field->mutable_value() = answers[i].value;
        any = true;
      }
      return any;
    }
    if (resolved.targets.empty() || answers.empty() || !answers[0].found) return false;
    PB::Facts::Value current = answers[0].value;
    for (const std::string &segment : resolved.targets[0].remainder) {
      if (!current.has_object_value()) return false;
      const PB::Facts::Value *child = nscapi::facts::tree::get(current.object_value(), segment);
      if (child == nullptr) return false;
      const PB::Facts::Value found = *child;
      current = found;
    }
    out = current;
    return true;
  }

  // The cached answer for a declared id (and parameter), if it is still
  // fresh.
  bool get_cached(const std::string &id, const std::string &parameter, const clock::time_point now, answer &out) const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    const std::map<cache_key, cached>::const_iterator it = cache_.find(cache_key(id, parameter));
    if (it == cache_.end() || now >= it->second.expires) return false;
    out = it->second.value;
    return true;
  }

  // Keep an answer. One that failed is not kept: the next reader asks again
  // rather than being told about a failure that may be over. An answer that
  // came with a note against it (a listing that was truncated) is an answer,
  // and is kept with the note - it is the large ones that most need the
  // cache.
  void store(const std::string &id, const std::string &parameter, const answer &value, const clock::time_point now) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    const cache_key key(id, parameter);
    if (!value.found) {
      cache_.erase(key);
      return;
    }
    // Only for something still declared: a module unloaded while it was being
    // asked must not leave its answer behind.
    if (declared_.find(id) == declared_.end()) return;
    for (std::map<cache_key, cached>::iterator it = cache_.begin(); it != cache_.end();) {
      it = now >= it->second.expires ? cache_.erase(it) : std::next(it);
    }
    while (cache_.size() >= max_cache_entries) {
      std::map<cache_key, cached>::iterator oldest = cache_.begin();
      for (std::map<cache_key, cached>::iterator it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.expires < oldest->second.expires) oldest = it;
      }
      cache_.erase(oldest);
    }
    cached c;
    c.value = value;
    c.expires = now + cache_ttl;
    cache_[key] = c;
  }

  // Forget every cached answer: a manual refresh means "read it again".
  void clear_cache() {
    boost::unique_lock<boost::mutex> lock(mutex_);
    cache_.clear();
  }

  // Held by the caller around a collection, so two readers of one set collect
  // once: the second finds the first one's answer in the cache.
  boost::mutex &collect_mutex() { return collect_mutex_; }

  // A parameter is taken verbatim from the consumer's path, so the one thing
  // checked here is that it is printable and bounded. What it *means* is the
  // producer's to decide, by matching it against what it enumerates.
  static bool is_valid_parameter(const std::string &parameter) {
    if (parameter.empty() || parameter.size() > max_parameter_length) return false;
    for (const char raw : parameter) {
      const unsigned char c = static_cast<unsigned char>(raw);
      if (c < 0x20 || c == 0x7f) return false;
    }
    return true;
  }

 private:
  struct entry {
    unsigned int plugin_id = 0;
    bool parameterized = false;
  };
  typedef std::pair<std::string, std::string> cache_key;
  struct cached {
    answer value;
    clock::time_point expires;
  };

  static std::vector<std::string> split(const std::string &path) {
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (true) {
      const std::size_t next = path.find('.', start);
      segments.push_back(path.substr(start, next == std::string::npos ? std::string::npos : next - start));
      if (next == std::string::npos) break;
      start = next + 1;
    }
    return segments;
  }

  static bool valid_segments(const std::vector<std::string> &segments) {
    for (const std::string &segment : segments) {
      if (!fact_repository::is_valid_key(segment)) return false;
    }
    return true;
  }

  static std::string clip(const std::string &text) { return text.size() <= 64 ? text : text.substr(0, 64) + "..."; }

  bool covers_locked(const std::string &head) const {
    if (declared_.find(head) != declared_.end()) return true;
    const std::string prefix = head + ".";
    const std::map<std::string, entry>::const_iterator it = declared_.lower_bound(prefix);
    return it != declared_.end() && it->first.compare(0, prefix.size(), prefix) == 0;
  }

  // `set` and `set.key` cannot both be declared: reading `set` would have two
  // answers. Something this plugin itself declared before does not count - it
  // is being replaced.
  bool shape_conflict_locked(const std::vector<std::string> &segments, const unsigned int plugin_id) const {
    if (segments.size() == 2) {
      const std::map<std::string, entry>::const_iterator whole = declared_.find(segments[0]);
      return whole != declared_.end() && whole->second.plugin_id != plugin_id;
    }
    const std::string prefix = segments[0] + ".";
    for (std::map<std::string, entry>::const_iterator it = declared_.lower_bound(prefix); it != declared_.end(); ++it) {
      if (it->first.compare(0, prefix.size(), prefix) != 0) break;
      if (it->second.plugin_id != plugin_id) return true;
    }
    return false;
  }

  // `rest` is the path below the declared id; `has_rest` tells an empty rest
  // (`pdh.instances.`) from none at all.
  static void add_target_locked(plan &result, const std::map<std::string, entry>::const_iterator &declared, const std::string &rest, const bool has_rest) {
    target t;
    t.id = declared->first;
    t.plugin_id = declared->second.plugin_id;
    if (declared->second.parameterized) {
      if (!has_rest || !is_valid_parameter(rest)) {
        result.error_id = declared->first;
        result.error = "'" + declared->first + "' takes a parameter: " + declared->first + ".<name>, at most " + std::to_string(max_parameter_length) +
                       " printable characters";
        return;
      }
      t.parameter = rest;
    } else if (has_rest) {
      t.remainder = split(rest);
    }
    result.targets.push_back(t);
  }

  void drop_cached_locked(const std::string &id) {
    for (std::map<cache_key, cached>::iterator it = cache_.begin(); it != cache_.end();) {
      it = it->first.first == id ? cache_.erase(it) : std::next(it);
    }
  }

  mutable boost::mutex mutex_;
  boost::mutex collect_mutex_;
  std::map<std::string, entry> declared_;
  std::set<unsigned int> seen_;
  std::map<cache_key, cached> cache_;
};

typedef std::shared_ptr<on_demand_facts> on_demand_facts_instance;

}  // namespace core
}  // namespace nsclient
