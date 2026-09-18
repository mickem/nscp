// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "error_handler.hpp"

void error_handler::add_message(bool is_error, const log_entry &message) {
  {
    const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return;
    log_entries.push_back(message);
    // Drop from the front rather than refusing the new entry: the buffer backs
    // a log *viewer*, and a viewer that stopped updating a thousand lines ago
    // is worse than one that cannot scroll all the way back. The error tally
    // and last_error_ below are deliberately not rewound - they count what the
    // agent reported, not what is still buffered.
    while (log_entries.size() > max_entries_) {
      log_entries.pop_front();
    }
    if (is_error) {
      error_count_++;
      last_error_ = message.message;
    }
  }
}
void error_handler::set_max_entries(std::size_t value) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return;
  max_entries_ = value == 0 ? 1 : value;
  while (log_entries.size() > max_entries_) {
    log_entries.pop_front();
  }
}
void error_handler::reset() {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return;
  log_entries.clear();
  last_error_ = "";
  error_count_ = 0;
}
error_handler::status error_handler::get_status() {
  status ret;
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return ret;
  ret.error_count = error_count_;
  ret.last_error = last_error_;
  return ret;
}
error_handler::log_list error_handler::get_messages(std::list<std::string> levels, std::size_t position, std::size_t ipp, std::size_t &count) {
  log_list ret;
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return ret;
  if (levels.empty()) {
    count = log_entries.size();
    if (position >= count) {
      return ret;
    }
    if ((position + ipp) >= count) {
      ipp = count - position;
    }
    entry_store::const_iterator cit = log_entries.begin() + position;
    const entry_store::const_iterator end = log_entries.begin() + position + ipp;

    for (; cit != end; ++cit) {
      ret.push_back(*cit);
    }
  } else {
    std::size_t i = 0;
    for (const log_entry &e : log_entries) {
      if (std::find(levels.begin(), levels.end(), e.type) == levels.end()) {
        continue;
      }
      i++;
      if (i < position) {
        continue;
      }
      if (i <= position + ipp) {
        ret.push_back(e);
      }
    }
    count = i;
  }
  return ret;
}

error_handler::log_list error_handler::get_messages_since(std::size_t since, std::size_t position, std::size_t ipp, std::size_t &count) {
  log_list ret;
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return ret;
  std::size_t i = 0;
  for (const log_entry &e : log_entries) {
    if (e.index <= since) {
      continue;
    }
    i++;
    if (i < position) {
      continue;
    }
    if (i <= position + ipp) {
      ret.push_back(e);
    }
  }
  count = i;
  return ret;
}
