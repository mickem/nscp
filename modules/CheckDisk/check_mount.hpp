// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

namespace check_mount_command {

// One entry from the system mount table.
struct mount_point {
  std::string mount;    // mount point (directory)
  std::string device;   // backing device / source
  std::string fstype;   // filesystem type
  std::string options;  // comma-separated mount options
};

// Read the current mount table (/proc/mounts on Linux). Returns an empty list
// on platforms without a mount table.
std::vector<mount_point> read_mount_points();

// Evaluate the mount table against the requested mount/options/fstype. Exposed
// (with an explicit mount table) for unit testing.
void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::vector<mount_point> &mounts);

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mount_command
