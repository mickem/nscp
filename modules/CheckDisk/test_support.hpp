/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

// Header-only test utilities shared between CheckDisk unit tests.
//
// Pulling these helpers into a single translation-unit-local header keeps the
// individual *_test.cpp files focused on the behaviour they exercise and
// avoids the maintenance cost of multiple drifting copies of the same RAII /
// protobuf glue. The header is `inline`-friendly and safe to include from
// any number of test files.

#include <boost/filesystem.hpp>
#include <fstream>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace check_disk_test_support {

// RAII helper that creates a unique scratch directory under the OS temp
// folder and removes it (including all contents) on destruction. Tests use
// it to make their fixtures hermetic — no test depends on any pre-existing
// files on the host, and a failing test cannot leak state into the next
// one.
class ScratchDir {
 public:
  // The prefix is embedded in the directory name so a leaked scratch
  // directory (e.g. from a debugger session) is easy to identify and
  // remove manually.
  explicit ScratchDir(const std::string &prefix = "nscp-test") {
    const std::string pattern = prefix + "-%%%%-%%%%";
    base_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path(pattern);
    boost::filesystem::create_directories(base_);
  }
  ~ScratchDir() {
    boost::system::error_code ec;
    boost::filesystem::remove_all(base_, ec);
  }
  ScratchDir(const ScratchDir &) = delete;
  ScratchDir &operator=(const ScratchDir &) = delete;

  const boost::filesystem::path &path() const { return base_; }
  std::string string() const { return base_.string(); }

  // Create a file under the scratch directory with the given relative name
  // and contents. Returns the absolute path so the caller can pass it
  // straight to the code under test. Subdirectories in `relative_name`
  // are created on demand.
  std::string make_file(const std::string &relative_name, const std::string &contents = "hello") const {
    const boost::filesystem::path p = base_ / relative_name;
    if (p.has_parent_path()) {
      boost::filesystem::create_directories(p.parent_path());
    }
    std::ofstream ofs(p.string(), std::ios::binary);
    ofs << contents;
    ofs.close();
    return p.string();
  }

  // Convenience alias when the caller does not care about the returned
  // absolute path (e.g. when they only want the file to exist for a
  // recursive scan to discover).
  void touch(const std::string &relative_name, const std::string &contents = "x") const { (void)make_file(relative_name, contents); }

 private:
  boost::filesystem::path base_;
};

// Concatenate every line message in a query response into a single string
// so tests can assert on the rendered output regardless of how the filter
// chose to split it across `Line` entries.
inline std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

}  // namespace check_disk_test_support
