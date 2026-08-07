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

  // Set (or, with an empty value, remove) a tag. Returns true when the
  // repository actually changed; the revision is only bumped in that case,
  // so re-setting the same value is a cheap no-op for pollers.
  bool set(const std::string &key, const std::string &value) {
    if (key.empty()) return false;
    boost::unique_lock<boost::mutex> lock(mutex_);
    const tag_map::iterator it = tags_.find(key);
    if (value.empty()) {
      if (it == tags_.end()) return false;
      tags_.erase(it);
    } else {
      if (it != tags_.end() && it->second == value) return false;
      tags_[key] = value;
    }
    ++revision_;
    return true;
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
