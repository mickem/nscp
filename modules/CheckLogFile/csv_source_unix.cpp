// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_csv's local reader on POSIX. A path the agent cannot open, a
// directory, or a file over the size limit is reported, never half-read.

#include <sys/stat.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

#include "check_csv.hpp"

namespace check_csv_command {

bool read_local_file(const std::string &path, const std::uint64_t max_bytes, std::string &bytes, std::string &error) {
  struct stat st {};
  if (::stat(path.c_str(), &st) != 0) {
    error = "Failed to open " + path + ": " + std::strerror(errno);
    return false;
  }
  if (S_ISDIR(st.st_mode)) {
    error = "Not a file: " + path;
    return false;
  }
  if (max_bytes > 0 && static_cast<std::uint64_t>(st.st_size) > max_bytes) {
    error = "CSV is larger than max-size (" + std::to_string(max_bytes) + " bytes): " + path;
    return false;
  }
  std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
  if (!in.is_open()) {
    error = "Failed to open " + path + ": " + std::strerror(errno);
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (in.bad()) {
    error = "Failed to read " + path;
    return false;
  }
  bytes = buffer.str();
  // The file may have grown between the stat and the read.
  if (max_bytes > 0 && bytes.size() > max_bytes) {
    error = "CSV is larger than max-size (" + std::to_string(max_bytes) + " bytes): " + path;
    return false;
  }
  return true;
}

}  // namespace check_csv_command
