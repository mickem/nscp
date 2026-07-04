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

#include "check_disk_health.hpp"

#include <map>
#include <set>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace disk_health_check {

namespace {
void copy_io(disk_health &h, const disk_io_check::disk_io &io) {
  h.read_bytes_per_sec = io.read_bytes_per_sec;
  h.write_bytes_per_sec = io.write_bytes_per_sec;
  h.reads_per_sec = io.reads_per_sec;
  h.writes_per_sec = io.writes_per_sec;
  h.queue_length = io.queue_length;
  h.percent_disk_time = io.percent_disk_time;
  h.percent_idle_time = io.percent_idle_time;
  h.split_io_per_sec = io.split_io_per_sec;
}
}  // namespace

// Join disk I/O (keyed by physical device) with disk free space (keyed by
// mountpoint / logical disk). On Windows both sides key off the same logical
// disk (e.g. "C:") so the join is direct. On Unix a free entry carries the
// backing device (e.g. "sda" for "/" mounted from /dev/sda1); we match the
// free entry against the I/O of that device so each mountpoint row reports both
// its free space and the I/O of the disk it lives on. I/O rows with no matching
// filesystem (e.g. "_Total", or an unmounted disk) are emitted as I/O-only rows.
health_type join(const disk_io_check::disks_type &io_data, const disk_free_check::drives_type &free_data) {
  std::map<std::string, const disk_io_check::disk_io *> io_by_name;
  for (const auto &io : io_data) {
    io_by_name[io.name] = &io;
  }

  health_type result;
  std::set<std::string> matched_io;

  // One row per filesystem, attaching the I/O of its backing device when known.
  for (const auto &df : free_data) {
    disk_health h;
    h.name = df.name;
    h.has_space = true;
    h.total = df.total;
    h.free = df.free;
    h.user_free = df.user_free;
    const std::string io_key = df.device.empty() ? df.name : df.device;
    const auto it = io_by_name.find(io_key);
    if (it != io_by_name.end()) {
      copy_io(h, *it->second);
      matched_io.insert(io_key);
    }
    result.push_back(h);
  }

  // I/O-only rows (devices/totals with no matching filesystem). has_space stays
  // false: there is no real space data, so the check must not treat the zeroed
  // total/free as a full disk.
  for (const auto &io : io_data) {
    if (matched_io.count(io.name)) continue;
    disk_health h;
    h.name = io.name;
    copy_io(h, io);
    result.push_back(h);
  }
  return result;
}

namespace check {

typedef disk_health filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Drive name (e.g. C:, D:, _Total)");

  // Guard for threshold expressions: I/O-only rows carry no space data.
  // no_perf: a boolean guard is not a metric (it would otherwise be emitted as
  // perf data because the default warn/crit expressions reference it).
  registry_
      .add_int_var("has_space", &filter_obj::get_has_space,
                   "1 if the row has filesystem space data, 0 for I/O-only rows (e.g. _Total or a disk with no mounted filesystem)")
      .no_perf();

  // Space metrics
  registry_.add_int_var("total", &filter_obj::get_total, "Total disk size in bytes")
      .add_int_var("free", &filter_obj::get_free, "Free disk space in bytes")
      .add_int_var("used", &filter_obj::get_used, "Used disk space in bytes")
      .add_int_var("user_free", &filter_obj::get_user_free, "Free disk space available to current user in bytes")
      .add_int_perf("B")
      .add_int_var("free_pct", &filter_obj::get_free_pct, "Percentage of free disk space")
      .add_int_perf("%")
      .add_int_var("used_pct", &filter_obj::get_used_pct, "Percentage of used disk space")
      .add_int_perf("%")

      // I/O metrics
      .add_int_var("read_bytes_per_sec", &filter_obj::get_read_bytes_per_sec, "Bytes read per second")
      .add_int_var("write_bytes_per_sec", &filter_obj::get_write_bytes_per_sec, "Bytes written per second")
      .add_int_var("total_bytes_per_sec", &filter_obj::get_total_bytes_per_sec, "Total bytes per second (read + write)")
      .add_int_var("reads_per_sec", &filter_obj::get_reads_per_sec, "Read IOPS")
      .add_int_var("writes_per_sec", &filter_obj::get_writes_per_sec, "Write IOPS")
      .add_int_var("iops", &filter_obj::get_iops, "Total IOPS (reads + writes)")
      .add_int_var("queue_length", &filter_obj::get_queue_length, "Current disk queue length")
      .add_int_perf("")
      .add_int_var("percent_disk_time", &filter_obj::get_percent_disk_time, "Percent of time the disk is busy")
      .add_int_perf("%")
      .add_int_var("percent_idle_time", &filter_obj::get_percent_idle_time, "Percent of time the disk is idle")
      .add_int_var("split_io_per_sec", &filter_obj::get_split_io_per_sec, "Split I/O operations per second");
}

void check_disk_health(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, health_type data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // Only judge free space on rows that actually have space data; I/O-only rows
  // (has_space = 0) would otherwise read as free_pct=0 and falsely go critical.
  filter_helper.add_options("(has_space = 1 and free_pct < 20) or percent_disk_time > 80", "(has_space = 1 and free_pct < 10) or percent_disk_time > 95",
                            "name != '_Total'", filter.get_filter_syntax(), "critical");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${free_pct}% free, ${percent_disk_time}% busy, q=${queue_length} iops=${iops}", "${name}", "",
                           "%(status): All disks are healthy.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;
  for (const disk_health &d : data) {
    const std::shared_ptr<filter_obj> record(new filter_obj(d));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace disk_health_check
