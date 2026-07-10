// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>

namespace uncpath_check {

// Free-space of a single UNC path (e.g. \\server\share). check_drivesize only
// sees OS-mounted drives; this check takes an arbitrary UNC path, optionally
// authenticates with alternate credentials, and reports quota-aware free space.
struct unc_obj {
  std::string path;
  long long size;       // total bytes on the share
  long long free;       // total free bytes on the share
  long long user_free;  // free bytes available to the querying user (quota-aware)
  bool ok;              // false if the path could not be queried
  std::string error;    // populated when ok == false

  unc_obj() : size(0), free(0), user_free(0), ok(false) {}

  std::string get_path() const { return path; }
  long long get_size() const { return size; }
  long long get_free() const { return free; }
  long long get_used() const { return size - free; }
  long long get_user_free() const { return user_free; }
  long long get_free_pct() const { return size == 0 ? 0 : (free * 100 / size); }
  long long get_used_pct() const { return size == 0 ? 0 : ((size - free) * 100 / size); }
  std::string get_free_human() const;
  std::string get_used_human() const;
  std::string get_size_human() const;

  std::string show() const { return path; }
};

// Platform data acquisition: query free space for `path`, optionally connecting
// with (user, password) first. On Windows uses WNetAddConnection2 +
// GetDiskFreeSpaceEx; on Unix statvfs (only for locally-mounted paths).
unc_obj query(const std::string &path, const std::string &user, const std::string &password);

namespace check {
void check_uncpath(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check

}  // namespace uncpath_check
