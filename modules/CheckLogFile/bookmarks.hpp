// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <map>
#include <string>

#include "bookmark_state.hpp"

namespace check_logfile {

// Per-(bookmark, file) read positions, shared by all concurrently executing
// check_logfile queries. Mirrors CheckEventLog's `bookmarks`: the map is the
// live state, the core storage is only used to persist it across restarts.
class bookmarks {
 public:
  typedef std::map<std::string, std::string> map_type;

  void add(const std::string &key, const std::string &value);
  // Returns an invalid position when the key is unknown (or unparsable).
  bookmark::position get(const std::string &key);
  map_type get_copy();

  // Key under which the position for `file` is stored for a given bookmark.
  static std::string make_key(const std::string &bookmark, const std::string &file);

 private:
  boost::timed_mutex mutex_;
  map_type bookmarks_;
};

}  // namespace check_logfile
