// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <boost/filesystem/path.hpp>
#include <list>

#include "filter.hpp"

struct runtime_data {
  struct transient_data_impl {
    std::string path_;
    transient_data_impl(std::string path) : path_(path) {}
    std::string to_string() const { return path_; }
  };
  typedef logfile_filter::filter filter_type;
  typedef std::shared_ptr<transient_data_impl> transient_data_type;

  struct file_container {
    boost::filesystem::path file;
    boost::uintmax_t size;
    std::time_t time;
    file_container() : size(0), time(0) {}
  };

  std::list<file_container> files;
  std::string column_split;
  std::string line_split;
  bool read_from_start;
  bool check_time;

  runtime_data() : read_from_start(false), check_time(false) {}
  void boot() {}
  void touch(boost::posix_time::ptime now);
  bool has_changed(transient_data_type) const;
  modern_filter::match_result process_item(filter_type &filter, transient_data_type);
  void set_split(std::string line, std::string column);
  void set_read_from_start(bool read_from_start_);
  void set_comparison(bool check_time_);

  void add_file(const boost::filesystem::path &path);

 private:
  bool has_changed(const file_container &fc) const;
};