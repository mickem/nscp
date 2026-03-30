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
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace disk_health_check {

health_type join(const disk_io_check::disks_type &io_data, const disk_free_check::drives_type &free_data) {
  std::map<std::string, disk_health> by_name;

  for (const auto &io : io_data) {
    disk_health &h = by_name[io.name];
    h.name = io.name;
    h.read_bytes_per_sec = io.read_bytes_per_sec;
    h.write_bytes_per_sec = io.write_bytes_per_sec;
    h.reads_per_sec = io.reads_per_sec;
    h.writes_per_sec = io.writes_per_sec;
    h.queue_length = io.queue_length;
    h.percent_disk_time = io.percent_disk_time;
    h.percent_idle_time = io.percent_idle_time;
    h.split_io_per_sec = io.split_io_per_sec;
  }

  for (const auto &df : free_data) {
    disk_health &h = by_name[df.name];
    h.name = df.name;
    h.total = df.total;
    h.free = df.free;
    h.user_free = df.user_free;
  }

  health_type result;
  for (const auto &kv : by_name) {
    result.push_back(kv.second);
  }
  return result;
}

namespace check {

typedef disk_health filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("name", &filter_obj::get_name, "Drive name (e.g. C:, D:, _Total)");

  // Space metrics
  registry_.add_int_x("total", &filter_obj::get_total, "Total disk size in bytes")
      .add_int_x("free", &filter_obj::get_free, "Free disk space in bytes")
      .add_int_x("used", &filter_obj::get_used, "Used disk space in bytes")
      .add_int_x("user_free", &filter_obj::get_user_free, "Free disk space available to current user in bytes")
      .add_int_perf("B")
      .add_int_x("free_pct", &filter_obj::get_free_pct, "Percentage of free disk space")
      .add_int_perf("%")
      .add_int_x("used_pct", &filter_obj::get_used_pct, "Percentage of used disk space")
      .add_int_perf("%")

      // I/O metrics
      .add_int_x("read_bytes_per_sec", &filter_obj::get_read_bytes_per_sec, "Bytes read per second")
      .add_int_x("write_bytes_per_sec", &filter_obj::get_write_bytes_per_sec, "Bytes written per second")
      .add_int_x("total_bytes_per_sec", &filter_obj::get_total_bytes_per_sec, "Total bytes per second (read + write)")
      .add_int_x("reads_per_sec", &filter_obj::get_reads_per_sec, "Read IOPS")
      .add_int_x("writes_per_sec", &filter_obj::get_writes_per_sec, "Write IOPS")
      .add_int_x("iops", &filter_obj::get_iops, "Total IOPS (reads + writes)")
      .add_int_x("queue_length", &filter_obj::get_queue_length, "Current disk queue length")
      .add_int_perf("")
      .add_int_x("percent_disk_time", &filter_obj::get_percent_disk_time, "Percent of time the disk is busy")
      .add_int_perf("%")
      .add_int_x("percent_idle_time", &filter_obj::get_percent_idle_time, "Percent of time the disk is idle")
      .add_int_x("split_io_per_sec", &filter_obj::get_split_io_per_sec, "Split I/O operations per second");
}

void check_disk_health(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, health_type data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  filter_helper.add_options("free_pct < 20 or percent_disk_time > 80", "free_pct < 10 or percent_disk_time > 95", "name != '_Total'",
                            filter.get_filter_syntax(), "critical");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${free_pct}% free, ${percent_disk_time}% busy, q=${queue_length} iops=${iops}", "${name}", "",
                           "%(status): All disks are healthy.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;
  for (const disk_health &d : data) {
    const boost::shared_ptr<filter_obj> record(new filter_obj(d));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace disk_health_check
