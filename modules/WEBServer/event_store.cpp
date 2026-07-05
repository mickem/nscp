// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "event_store.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

void event_store::add(const std::string &event, const std::map<std::string, std::string> &data) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return;
  event_entry e;
  e.index = next_index_++;
  e.event = event;
  e.date = boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time());
  e.data = data;
  entries_.push_back(std::move(e));
  while (entries_.size() > max_entries_) {
    entries_.pop_front();
  }
}

event_store::event_list event_store::list() const {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return event_list();
  return entries_;
}

event_store::event_list event_store::pop_all() {
  event_list ret;
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return ret;
  ret.swap(entries_);
  return ret;
}

std::size_t event_store::size() const {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return 0;
  return entries_.size();
}

void event_store::set_max_entries(std::size_t value) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return;
  max_entries_ = value == 0 ? 1 : value;
  while (entries_.size() > max_entries_) {
    entries_.pop_front();
  }
}
