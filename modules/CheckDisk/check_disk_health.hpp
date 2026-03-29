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

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <string>

#include "check_disk_io.hpp"

namespace disk_health_check {

struct disk_health {
  std::string name;

  // Space metrics (from disk_free_check)
  long long total;
  long long free;
  long long user_free;

  // I/O metrics (from disk_io_check)
  long long read_bytes_per_sec;
  long long write_bytes_per_sec;
  long long reads_per_sec;
  long long writes_per_sec;
  long long queue_length;
  long long percent_disk_time;
  long long percent_idle_time;
  long long split_io_per_sec;

  disk_health()
      : total(0),
        free(0),
        user_free(0),
        read_bytes_per_sec(0),
        write_bytes_per_sec(0),
        reads_per_sec(0),
        writes_per_sec(0),
        queue_length(0),
        percent_disk_time(0),
        percent_idle_time(0),
        split_io_per_sec(0) {}
  disk_health(const disk_health &other) = default;
  disk_health &operator=(const disk_health &other) = default;

  // Space accessors
  std::string get_name() const { return name; }
  long long get_total() const { return total; }
  long long get_free() const { return free; }
  long long get_used() const { return total - free; }
  long long get_user_free() const { return user_free; }
  long long get_free_pct() const { return total == 0 ? 0 : (free * 100 / total); }
  long long get_used_pct() const { return total == 0 ? 0 : ((total - free) * 100 / total); }

  // I/O accessors
  long long get_read_bytes_per_sec() const { return read_bytes_per_sec; }
  long long get_write_bytes_per_sec() const { return write_bytes_per_sec; }
  long long get_total_bytes_per_sec() const { return read_bytes_per_sec + write_bytes_per_sec; }
  long long get_reads_per_sec() const { return reads_per_sec; }
  long long get_writes_per_sec() const { return writes_per_sec; }
  long long get_iops() const { return reads_per_sec + writes_per_sec; }
  long long get_queue_length() const { return queue_length; }
  long long get_percent_disk_time() const { return percent_disk_time; }
  long long get_percent_idle_time() const { return percent_idle_time; }
  long long get_split_io_per_sec() const { return split_io_per_sec; }

  std::string show() const { return name; }
};

typedef std::list<disk_health> health_type;

/// Join disk I/O and disk free data by drive name (outer join).
/// Drives present in only one source get zeroed fields for the missing side.
health_type join(const disk_io_check::disks_type &io_data, const disk_free_check::drives_type &free_data);

namespace check {
void check_disk_health(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                       health_type data);
}  // namespace check

}  // namespace disk_health_check

