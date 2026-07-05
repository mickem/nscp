// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <list>

#include "filter.hpp"

struct runtime_data {
  typedef eventlog_filter::filter filter_type;
  typedef eventlog_filter::filter::object_type transient_data_type;

  int truncate_;
  std::list<std::string> files;

  runtime_data() : truncate_(0) {}
  runtime_data(int truncate) : truncate_(truncate) {}
  void boot() {}
  void touch(boost::posix_time::ptime) {}
  bool has_changed(transient_data_type record) const;
  modern_filter::match_result process_item(filter_type &filter, transient_data_type data);
  void add_file(const std::string &file);
};