// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_disk_health.hpp"

#include <boost/optional.hpp>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/format_functions.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>

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
  h.read_latency = io.read_latency;
  h.write_latency = io.write_latency;
  h.total_latency = io.total_latency;
}
}  // namespace

// Join disk I/O (keyed by physical device) with disk free space (keyed by
// mountpoint / logical disk). On Windows both sides key off the same logical
// disk (e.g. "C:") so the join is direct. On Unix a free entry carries the
// backing device (e.g. "sda" for "/" mounted from /dev/sda1); we match the
// free entry against the I/O of that device so each mountpoint row reports both
// its free space and the I/O of the disk it lives on. I/O rows with no matching
// filesystem (e.g. "_Total", or an unmounted disk) are emitted as I/O-only rows.
health_type join(const disk_io_check::disks_type &io_data, const disk_free_check::drives_type &free_data,
                 const disk_device_check::devices_type &device_data) {
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

  // Physical-disk device-state rows (Windows). These are per-physical-disk, not
  // per-filesystem, so they are appended as their own rows (has_space=false,
  // has_device=true) rather than joined onto the space/IO rows — mapping a
  // logical drive back to its physical disk is unreliable across storage
  // configurations (spanned volumes, storage spaces, dynamic disks).
  for (const auto &dev : device_data) {
    disk_health h;
    h.name = dev.friendly_name.empty() ? ("Disk " + std::to_string(dev.number)) : dev.friendly_name;
    h.has_device = true;
    h.disk_number = dev.number;
    h.friendly_name = dev.friendly_name;
    h.serial = dev.serial;
    h.media_type = dev.media_type;
    h.health_status = dev.health_status;
    h.operational_status = dev.get_operational_status();
    h.is_offline = dev.is_offline;
    h.is_readonly = dev.is_readonly;
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

namespace {
// What a space keyword renders as, and compares equal to, on a row with no
// filesystem behind it (`free_pct = 'no space data'` is the presence test).
const char *no_space = "no space data";

typedef long long (filter_obj::*space_getter)() const;

// Wraps a space accessor so it only yields a value on rows that actually have
// filesystem data; every other row reports "no value" (see no_space above).
std::function<boost::optional<long long>(std::shared_ptr<filter_obj>)> space_value(const space_getter getter) {
  return [getter](const std::shared_ptr<filter_obj> obj) -> boost::optional<long long> {
    if (!obj->has_space) return boost::none;
    return ((*obj).*getter)();
  };
}

// Rendering counterpart of the above: bytes as a human-readable string,
// percentages with their sign, and a dash where there is no space data.
std::function<std::string(std::shared_ptr<filter_obj>, parsers::where::evaluation_context)> space_bytes_human(const space_getter getter) {
  return [getter](const std::shared_ptr<filter_obj> obj, const parsers::where::evaluation_context context) -> std::string {
    if (!obj->has_space) return "-";
    return str::format::format_byte_units(((*obj).*getter)(), context->get_number_format());
  };
}

std::function<std::string(std::shared_ptr<filter_obj>)> space_pct_human(const space_getter getter) {
  return [getter](const std::shared_ptr<filter_obj> obj) -> std::string {
    if (!obj->has_space) return "-";
    return str::xtos(((*obj).*getter)()) + "%";
  };
}
}  // namespace

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Drive name (e.g. C:, D:, _Total)");

  // Guard for threshold expressions: I/O-only rows carry no space data.
  // no_perf: a boolean guard is not a metric (it would otherwise be emitted as
  // perf data because the default warn/crit expressions reference it).
  registry_
      .add_int_var("has_space", &filter_obj::get_has_space,
                   "1 if the row has filesystem space data, 0 for I/O-only rows (e.g. _Total or a disk with no mounted filesystem)")
      .no_perf();

  // Space metrics. free_pct is this check's primary metric and keeps the bare
  // perf-syntax alias (the drive name, as check_drivesize graphs it); the rest
  // carry their own suffix so no two share a label.
  //
  // Optional rather than plain ints: an I/O-only or device row
  // has no filesystem behind it, and reporting its zeroed total as `0% free`
  // both read as a full disk in the message and recorded a flat 0% in the
  // graphs (#1392). As optionals they render as `-`, compare false against
  // every numeric threshold, and emit no perfdata on those rows.
  registry_.add_optional_int_var("size", space_value(&filter_obj::get_total), no_space, "Total disk size in bytes (I/O-only rows have none)")
      .add_int_perf("B", "", "_total")
      .add_optional_int_var("free", space_value(&filter_obj::get_free), no_space, "Free disk space in bytes (I/O-only rows have none)")
      .add_int_perf("B", "", "_free")
      .add_optional_int_var("used", space_value(&filter_obj::get_used), no_space, "Used disk space in bytes (I/O-only rows have none)")
      .add_int_perf("B", "", "_used")
      .add_optional_int_var("user_free", space_value(&filter_obj::get_user_free), no_space,
                            "Free disk space available to current user in bytes (I/O-only rows have none)")
      .add_int_perf("B", "", "_user_free")
      .add_optional_int_var("free_pct", space_value(&filter_obj::get_free_pct), no_space, "Percentage of free disk space (I/O-only rows have none)")
      .add_int_perf("%")
      .add_optional_int_var("used_pct", space_value(&filter_obj::get_used_pct), no_space, "Percentage of used disk space (I/O-only rows have none)")
      .add_int_perf("%", "", "_used_pct");

  // Deprecated alias: `total` predates the rename to `size` (the name clashes
  // with the generic `total` summary keyword, which shadows it in top-syntax and
  // in the generated docs; `size` also matches check_drivesize). Kept so
  // existing filters keep working; same perf spec so existing graphs keep their
  // series.
  registry_
      .add_optional_int_var("total", space_value(&filter_obj::get_total), no_space,
                            "Deprecated alias for size (the name clashes with the generic total summary keyword).")
      .add_int_perf("B", "", "_total");

  // I/O metrics
  registry_.add_int_var("read_bytes_per_sec", &filter_obj::get_read_bytes_per_sec, "Bytes read per second")
      .add_int_var("write_bytes_per_sec", &filter_obj::get_write_bytes_per_sec, "Bytes written per second")
      .add_int_var("total_bytes_per_sec", &filter_obj::get_total_bytes_per_sec, "Total bytes per second (read + write)")
      .add_int_var("reads_per_sec", &filter_obj::get_reads_per_sec, "Read IOPS")
      .add_int_var("writes_per_sec", &filter_obj::get_writes_per_sec, "Write IOPS")
      .add_int_var("iops", &filter_obj::get_iops, "Total IOPS (reads + writes)")
      .add_int_var("queue_length", &filter_obj::get_queue_length, "Current disk queue length")
      .add_int_perf("", "", "_queue_length")
      .add_int_var("percent_disk_time", &filter_obj::get_percent_disk_time, "Percent of time the disk is busy")
      .add_int_perf("%", "", "_percent_disk_time")
      .add_int_var("percent_idle_time", &filter_obj::get_percent_idle_time, "Percent of time the disk is idle")
      .add_int_var("split_io_per_sec", &filter_obj::get_split_io_per_sec, "Split I/O operations per second")
      .add_float("read_latency", &filter_obj::get_read_latency, "Average read latency in milliseconds (over the collection interval)")
      .add_float_perf("ms", "", "_read_latency")
      .add_float("write_latency", &filter_obj::get_write_latency, "Average write latency in milliseconds (over the collection interval)")
      .add_float_perf("ms", "", "_write_latency")
      .add_float("total_latency", &filter_obj::get_total_latency, "Average latency per I/O (read + write) in milliseconds (over the collection interval)")
      .add_float_perf("ms", "", "_total_latency");

  // Physical-disk device state (Windows; MSFT_PhysicalDisk / MSFT_Disk). These
  // populate only on device rows (has_device = 1). has_device is a guard, not a
  // metric, so it emits no perfdata.
  registry_
      .add_int_var("has_device", &filter_obj::get_has_device, "1 if the row carries physical-disk device state (a per-disk row), 0 otherwise (guard; no perfdata)")
      .no_perf();
  registry_.add_string_var("friendly_name", &filter_obj::get_friendly_name, "Physical disk friendly name (device rows)")
      .add_string_var("serial", &filter_obj::get_serial, "Physical disk serial number (device rows)")
      .add_string_var("media_type", &filter_obj::get_media_type, "Physical disk media type: HDD, SSD, SCM or Unspecified (device rows)")
      .add_string_var("health_status", &filter_obj::get_health_status, "Physical disk health: Healthy, Warning, Unhealthy or Unknown (device rows)")
      .add_string_var("operational_status", &filter_obj::get_operational_status,
                      "Physical disk operational status, synthesised single value: Offline, OK, or the health string (device rows)");
  registry_.add_int_var("disk_number", &filter_obj::get_disk_number, "Physical disk number/index (device rows)")
      .no_perf()
      .add_int_var("is_offline", &filter_obj::get_is_offline, "1 if the physical disk is offline (device rows)")
      .no_perf()
      .add_int_var("is_readonly", &filter_obj::get_is_readonly, "1 if the physical disk is read-only (device rows)")
      .no_perf();

  // Rendering forms of the space keywords: `${free}` prints 12.3G rather than
  // a raw byte count, and a row with no filesystem prints `-` instead of a
  // fabricated zero. Thresholds and perfdata keep using the numeric variables
  // above, exactly as in check_drivesize.
  registry_.add_human_string_context("size", space_bytes_human(&filter_obj::get_total), "")
      .add_human_string_context("total", space_bytes_human(&filter_obj::get_total), "")  // deprecated alias for size
      .add_human_string_context("free", space_bytes_human(&filter_obj::get_free), "")
      .add_human_string_context("used", space_bytes_human(&filter_obj::get_used), "")
      .add_human_string_context("user_free", space_bytes_human(&filter_obj::get_user_free), "")
      .add_human_string("free_pct", space_pct_human(&filter_obj::get_free_pct), "")
      .add_human_string("used_pct", space_pct_human(&filter_obj::get_used_pct), "");

  // Byte-valued keywords with no arithmetic in the grammar: without these a
  // template can only print the raw count (#1392).
  parsers::where::format_functions::register_format_functions(registry_);
}

void check_disk_health(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, health_type data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // Only judge free space on rows that actually have space data; I/O-only rows
  // (has_space = 0) would otherwise read as free_pct=0 and falsely go critical.
  // Device rows (has_device = 1) are judged on physical-disk health/offline
  // state; space rows on free space; I/O rows on busy time. Each clause is
  // gated on its row kind so it only applies where that data is real.
  filter_helper.add_options(
      "(has_space = 1 and free_pct < 20) or percent_disk_time > 80 or (has_device = 1 and health_status = 'Warning')",
      "(has_space = 1 and free_pct < 10) or percent_disk_time > 95 or (has_device = 1 and (health_status = 'Unhealthy' or is_offline = 1))",
      "name != '_Total'", filter.get_filter_syntax(), "critical");
  // `${free_pct}` renders through the human form, which carries its own `%`
  // (and a `-` where the row has no filesystem).
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${free_pct} free, ${percent_disk_time}% busy, q=${queue_length} iops=${iops}", "${name}", "",
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
