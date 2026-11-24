// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <fstream>

#include "log_file.hpp"

namespace check_logfile {

std::unique_ptr<std::streambuf> open_log_file(const std::string &path, std::string &error) {
  std::unique_ptr<std::filebuf> buf(new std::filebuf());
  if (buf->open(path.c_str(), std::ios::in | std::ios::binary) == nullptr) {
    error = "Failed to open file: " + path;
    return nullptr;
  }
  return std::unique_ptr<std::streambuf>(buf.release());
}

}  // namespace check_logfile
