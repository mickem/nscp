// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <map>
#include <memory>
#include <string>

namespace nsclient {
namespace core {

// Central repository for host "tags": small key=value facts about this host
// contributed by modules (via NSAPISetTag) or the core itself - e.g.
// CheckDisk reporting `drives=c:,d:` or CheckSystem reporting
// `sqlserver=detected`. Consumers (the web UI, the fleet sync) read the full
// map; the monotonic revision lets pollers detect "something changed" without
// diffing the map themselves.
//
// Thread safety: set/get can be called from any plugin or core thread.
class tag_repository {
 public:
  typedef std::map<std::string, std::string> tag_map;

  // Bounds. Every tag is sent in full on every state report to the fleet server
  // and returned in every /api/v2/tags response, so an over-eager module (one
  // tag per process, per file system, per interface, ...) could bloat both with
  // no backpressure. Reject anything past these limits instead of growing
  // without bound; a rejected set leaves the map untouched, so an existing
  // tag is never clobbered by a value that could not be stored.
  static const std::size_t max_key_length = 128;
  static const std::size_t max_value_length = 1024;
  static const std::size_t max_tags = 256;

  // Result of a set(). A plain bool conflated two very different "false"
  // cases: a benign no-op (re-setting the same value, which modules do every
  // interval) and a tag that was actually dropped - which left a drop at the
  // tag cap invisible to the API layer and therefore unloggable.
  enum class set_result {
    changed,    // stored or removed; revision bumped
    unchanged,  // no-op: the same value again, or removing an absent key
    rejected    // empty/oversized key, oversized value, or repository full
  };

  // Set (or, with an empty value, remove) a tag. Returns `changed` when the
  // repository actually changed; the revision is only bumped in that case,
  // so re-setting the same value is a cheap no-op (`unchanged`) for pollers.
  // Returns `rejected` (no change) for an empty/oversized key, an oversized
  // value, or a new key that would exceed max_tags. A removal is always
  // allowed, so a host can always shed tags even at capacity.
  set_result set(const std::string &key, const std::string &value) {
    if (key.empty() || key.size() > max_key_length) return set_result::rejected;
    if (value.size() > max_value_length) return set_result::rejected;
    boost::unique_lock<boost::mutex> lock(mutex_);
    const tag_map::iterator it = tags_.find(key);
    if (value.empty()) {
      if (it == tags_.end()) return set_result::unchanged;
      tags_.erase(it);
    } else {
      if (it != tags_.end() && it->second == value) return set_result::unchanged;
      if (it == tags_.end() && tags_.size() >= max_tags) return set_result::rejected;
      tags_[key] = value;
    }
    ++revision_;
    return set_result::changed;
  }

  tag_map get_all() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return tags_;
  }

  // Monotonic change counter: starts at 0 (empty repository) and increments
  // on every effective set/remove.
  unsigned long long get_revision() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return revision_;
  }

 private:
  mutable boost::mutex mutex_;
  tag_map tags_;
  unsigned long long revision_ = 0;
};

typedef std::shared_ptr<tag_repository> tag_repository_instance;

}  // namespace core
}  // namespace nsclient
