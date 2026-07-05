// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread.hpp>
#include <cstddef>
#include <deque>
#include <map>
#include <string>

struct event_store {
  struct event_entry {
    std::size_t index;
    std::string event;
    std::string date;
    std::map<std::string, std::string> data;
  };

  typedef std::deque<event_entry> event_list;

  event_store() : next_index_(0), max_entries_(1000) {}

  void add(const std::string &event, const std::map<std::string, std::string> &data);
  event_list list() const;
  event_list pop_all();
  std::size_t size() const;
  void set_max_entries(std::size_t value);

 private:
  mutable boost::timed_mutex mutex_;
  event_list entries_;
  std::size_t next_index_;
  std::size_t max_entries_;
};
