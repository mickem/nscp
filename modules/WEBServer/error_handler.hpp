// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread.hpp>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#include "error_handler_interface.hpp"

struct error_handler : error_handler_interface {
  // Every log line in the agent lands here, and the only thing that empties
  // the buffer is an authenticated DELETE /api/v2/logs. Rejected requests log
  // an error, and a request is rejected *before* it is authenticated - so
  // anyone who can reach the port could grow this without bound, one heap
  // entry per request, for the life of the process. Hence a ring: the newest
  // entries are the ones the log viewer shows, and the oldest fall off. The
  // sibling stores are bounded the same way (event_store 1000, token_store
  // 4096).
  static constexpr std::size_t kDefaultMaxEntries = 1000;

  error_handler() : error_count_(0), max_entries_(kDefaultMaxEntries) {}
  void add_message(bool is_error, const log_entry &message);
  void reset();
  status get_status();
  log_list get_messages(std::list<std::string> levels, std::size_t position, std::size_t ipp, std::size_t &count) override;
  log_list get_messages_since(std::size_t since, std::size_t position, std::size_t ipp, std::size_t &count) override;

  // Lower the cap (tests). Never zero: a store that cannot hold the line it
  // was just handed is a store that reports nothing at all.
  void set_max_entries(std::size_t value);

 private:
  // A deque rather than the log_list vector the accessors return: entries are
  // dropped from the front on every overflow, which is O(n) on a vector.
  typedef std::deque<log_entry> entry_store;

  boost::timed_mutex mutex_;
  entry_store log_entries;
  std::string last_error_;
  unsigned int error_count_;
  std::size_t max_entries_;
};