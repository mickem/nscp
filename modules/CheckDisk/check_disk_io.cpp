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

// Platform-neutral parts of the disk I/O / disk free subsystem: the metric
// builders, the thread-safe data holders' get/set, and the check_disk_io filter.
// The per-platform data acquisition (fetch) lives in check_disk_io_win.cpp /
// check_disk_io_unix.cpp.

#include "check_disk_io.hpp"

#include <boost/thread/locks.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>

namespace disk_io_check {

void disk_io::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  add_metric(section, name + ".read_bytes_per_sec", read_bytes_per_sec);
  add_metric(section, name + ".write_bytes_per_sec", write_bytes_per_sec);
  add_metric(section, name + ".reads_per_sec", reads_per_sec);
  add_metric(section, name + ".writes_per_sec", writes_per_sec);
  add_metric(section, name + ".queue_length", queue_length);
  add_metric(section, name + ".percent_disk_time", percent_disk_time);
  add_metric(section, name + ".percent_idle_time", percent_idle_time);
  add_metric(section, name + ".split_io_per_sec", split_io_per_sec);
}

disks_type disk_io_data::get() {
  const boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading disk I/O data");
  return disks_;
}
void disk_io_data::set(const disks_type &disks) {
  const boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!write_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing disk I/O data");
  disks_ = disks;
}

namespace check {

typedef disk_io filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Logical disk name (e.g. C:, D:, _Total)");

  registry_.add_int_var("read_bytes_per_sec", &filter_obj::get_read_bytes_per_sec, "Bytes read per second")
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

void check_disk_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, disks_type data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  filter_helper.add_options("percent_disk_time > 80", "percent_disk_time > 95", "name != '_Total'", filter.get_filter_syntax(), "critical");
  filter_helper.add_syntax("${status}: ${list}",
                           "${name}: ${percent_disk_time}% busy, read=${read_bytes_per_sec}B/s write=${write_bytes_per_sec}B/s q=${queue_length}", "${name}",
                           "", "%(status): All disk I/O seems ok.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;
  for (const disk_io &d : data) {
    std::shared_ptr<filter_obj> record(new filter_obj(d));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace disk_io_check

namespace disk_free_check {

void disk_free::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  add_metric(section, name + ".total", total);
  add_metric(section, name + ".free", free);
  add_metric(section, name + ".used", total - free);
  add_metric(section, name + ".user_free", user_free);
  add_metric(section, name + ".free_pct", get_free_pct());
  add_metric(section, name + ".used_pct", get_used_pct());
}

void disk_free_data::set(const drives_type &drives) {
  const boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!write_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing disk free data");
  drives_ = drives;
}

drives_type disk_free_data::get() {
  const boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing disk free data");
  return drives_;
}

}  // namespace disk_free_check
