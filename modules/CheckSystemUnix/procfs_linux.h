// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Reading the small virtual files the Linux sources are built on: procfs,
// sysfs and the DMI tables.

#include <fstream>
#include <sstream>
#include <string>

namespace procfs {

// A whole (small, virtual) file. Empty when it is not there or cannot be
// read, which is the normal case for a DMI file in a container, a board with
// no SMBIOS, or a process that exited between the listing and the read.
inline std::string read_file(const std::string &path) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) return "";
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

}  // namespace procfs
