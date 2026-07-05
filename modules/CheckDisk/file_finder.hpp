// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>
#include <file_helpers.hpp>
#include <memory>
#include <string>
#include <vector>

#include "filter.hpp"

namespace file_finder {
bool is_directory(unsigned long dwAttr);

struct scanner_context {
  bool debug = false;
  std::string pattern;
  long long now;
  int max_depth;
  // Set to true when a user-supplied top-level path either does not exist or
  // cannot be opened. This lets the caller (CheckDisk::check_files) report
  // UNKNOWN with a useful message instead of silently returning OK / "No
  // files found" when the operator has misconfigured the path. See #613.
  std::vector<std::string> missing_paths;
  bool is_valid_level(int current_level) const;
  static void report_error(const std::string &str);
  void report_debug(const std::string &str) const;
  static void report_warning(const std::string &msg);
};

void recursive_scan(file_filter::filter &filter, scanner_context &context, const boost::filesystem::path &dir,
                    const std::shared_ptr<file_filter::filter_obj> &total_obj, bool total_all, bool recursive = false, int current_level = 0);

// Stat a single file path and return a filter_obj. Returns an empty
// shared_ptr if the path does not exist or cannot be opened. Used by the
// check_single_file command to inspect properties of one specific file
// without involving directory recursion or pattern matching.
std::shared_ptr<file_filter::filter_obj> stat_single_file(const boost::filesystem::path &path, long long now);
}  // namespace file_finder
