// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "bookmarks.hpp"

namespace check_logfile {

void bookmarks::add(const std::string &key, const std::string &value) {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) {
    return;
  }
  bookmarks_[key] = value;
}

bookmark::position bookmarks::get(const std::string &key) {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) {
    return bookmark::position();
  }
  const map_type::const_iterator cit = bookmarks_.find(key);
  if (cit == bookmarks_.end()) {
    return bookmark::position();
  }
  return bookmark::parse(cit->second);
}

bookmarks::map_type bookmarks::get_copy() {
  map_type ret;
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) {
    return ret;
  }
  ret.insert(bookmarks_.begin(), bookmarks_.end());
  return ret;
}

std::string bookmarks::make_key(const std::string &bookmark, const std::string &file) { return bookmark + "\x1f" + file; }

}  // namespace check_logfile
