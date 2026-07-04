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
