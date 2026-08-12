// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Windows data acquisition for the disk I/O / disk free subsystem (WMI PerfDisk
// + GetDiskFreeSpaceEx). The metric builders / check logic are shared in
// check_disk_io.cpp.

#include "check_disk_io.hpp"

#include <boost/thread/locks.hpp>
#include <map>
#include <nsclient/nsclient_exception.hpp>
#include <utility>

namespace disk_io_check {

// Win32_PerfFormattedData_PerfDisk_LogicalDisk provides pre-formatted per-second values.
// The "_Total" instance aggregates across all logical disks.
std::string helper::perf_query =
    "select Name, DiskReadBytesPersec, DiskWriteBytesPersec, DiskReadsPersec, DiskWritesPersec,"
    " CurrentDiskQueueLength, PercentDiskTime, PercentIdleTime, SplitIOPerSec"
    " from Win32_PerfFormattedData_PerfDisk_LogicalDisk";
// Raw PERF_AVERAGE_TIMER counters for latency: cumulative ticks spent in
// reads/writes/transfers plus the cumulative operation counts (the _Base
// counters). Average latency is delta(ticks) / frequency / delta(ops) between
// two samples; the formatted class cannot be used as it rounds to whole seconds.
std::string helper::raw_latency_query =
    "select Name, AvgDiskSecPerRead, AvgDiskSecPerRead_Base, AvgDiskSecPerWrite, AvgDiskSecPerWrite_Base,"
    " AvgDiskSecPerTransfer, AvgDiskSecPerTransfer_Base, Frequency_PerfTime"
    " from Win32_PerfRawData_PerfDisk_LogicalDisk";
std::string helper::perf_namespace = "root\\CIMV2";

namespace {
// PERF_AVERAGE_TIMER tick counters and their PERF_AVERAGE_BASE operation
// counters are 32-bit and wrap; deltas are therefore computed modulo 2^32.
unsigned long long delta32(const unsigned long long cur, const unsigned long long prev) { return (cur - prev) & 0xFFFFFFFFull; }

double avg_latency_ms(const unsigned long long cur_ticks, const unsigned long long prev_ticks, const unsigned long long cur_ops,
                      const unsigned long long prev_ops, const unsigned long long frequency) {
  const unsigned long long ops = delta32(cur_ops, prev_ops);
  if (ops == 0 || frequency == 0) return 0.0;
  return static_cast<double>(delta32(cur_ticks, prev_ticks)) * 1000.0 / static_cast<double>(frequency) / static_cast<double>(ops);
}
}  // namespace

void disk_io::read_wmi(const wmi_impl::row &r) {
  name = r.get_string("Name");
  read_bytes_per_sec = r.get_int("DiskReadBytesPersec");
  write_bytes_per_sec = r.get_int("DiskWriteBytesPersec");
  reads_per_sec = r.get_int("DiskReadsPersec");
  writes_per_sec = r.get_int("DiskWritesPersec");
  queue_length = r.get_int("CurrentDiskQueueLength");
  percent_disk_time = r.get_int("PercentDiskTime");
  percent_idle_time = r.get_int("PercentIdleTime");
  split_io_per_sec = r.get_int("SplitIOPerSec");
}

disks_type disk_io_data::query_perf() {
  wmi_impl::query wmi_q(helper::perf_query, helper::perf_namespace, "", "");
  wmi_impl::row_enumerator row = wmi_q.execute();
  disks_type disks;
  while (row.has_next()) {
    const wmi_impl::row r = row.get_next();
    disk_io d;
    d.read_wmi(r);
    disks.push_back(d);
  }
  return disks;
}

// Latency from the raw counters: average time per I/O since the previous
// sample. The first sample only seeds prev_raw_, leaving latencies at 0.
void disk_io_data::apply_latency(disks_type &disks) {
  std::map<std::string, disk_io *> by_name;
  for (disk_io &d : disks) by_name[d.name] = &d;

  wmi_impl::query raw_q(helper::raw_latency_query, helper::perf_namespace, "", "");
  wmi_impl::row_enumerator raw_row = raw_q.execute();
  std::map<std::string, raw_latency_sample> current;
  while (raw_row.has_next()) {
    const wmi_impl::row r = raw_row.get_next();
    const std::string name = r.get_string("Name");
    raw_latency_sample s;
    s.read_ticks = static_cast<unsigned long long>(r.get_int("AvgDiskSecPerRead"));
    s.read_ops = static_cast<unsigned long long>(r.get_int("AvgDiskSecPerRead_Base"));
    s.write_ticks = static_cast<unsigned long long>(r.get_int("AvgDiskSecPerWrite"));
    s.write_ops = static_cast<unsigned long long>(r.get_int("AvgDiskSecPerWrite_Base"));
    s.transfer_ticks = static_cast<unsigned long long>(r.get_int("AvgDiskSecPerTransfer"));
    s.transfer_ops = static_cast<unsigned long long>(r.get_int("AvgDiskSecPerTransfer_Base"));
    const auto frequency = static_cast<unsigned long long>(r.get_int("Frequency_PerfTime"));
    current[name] = s;

    const auto dit = by_name.find(name);
    const auto pit = prev_raw_.find(name);
    if (dit == by_name.end() || pit == prev_raw_.end()) continue;
    const raw_latency_sample &p = pit->second;
    dit->second->read_latency = avg_latency_ms(s.read_ticks, p.read_ticks, s.read_ops, p.read_ops, frequency);
    dit->second->write_latency = avg_latency_ms(s.write_ticks, p.write_ticks, s.write_ops, p.write_ops, frequency);
    dit->second->total_latency = avg_latency_ms(s.transfer_ticks, p.transfer_ticks, s.transfer_ops, p.transfer_ops, frequency);
  }
  prev_raw_.swap(current);
}

bool disk_io_data::fetch() {
  stored_data_ = false;
  if (!fetch_disk_io_) return false;

  disks_type disks;
  try {
    disks = query_perf();
  } catch (const wmi_impl::wmi_exception &e) {
    if (e.get_code() == WBEM_E_INVALID_QUERY || e.get_code() == WBEM_E_NOT_FOUND) {
      fetch_disk_io_ = false;
      throw nsclient::nsclient_exception("Failed to fetch disk I/O metrics (performance counter not available), disabling...");
    }
    throw nsclient::nsclient_exception("Failed to fetch disk I/O metrics: " + e.reason());
  }

  // A latency failure must not take down the rates: keep the formatted data
  // and report the problem; on a permanent error stop querying the raw class.
  if (fetch_latency_) {
    try {
      apply_latency(disks);
    } catch (const wmi_impl::wmi_exception &e) {
      if (e.get_code() == WBEM_E_INVALID_QUERY || e.get_code() == WBEM_E_NOT_FOUND) {
        fetch_latency_ = false;
        set(disks);
        throw nsclient::nsclient_exception("Failed to fetch disk latency (raw performance counter not available), disabling latency (rates still work)...");
      }
      set(disks);
      throw nsclient::nsclient_exception("Failed to fetch disk latency (rates still work): " + e.reason());
    }
  }
  set(disks);
  return true;
}

}  // namespace disk_io_check

namespace disk_device_check {

// Best-effort physical-disk device state. MSFT_PhysicalDisk carries
// media/health/serial/friendly-name; MSFT_Disk carries the offline/read-only
// flags. We key the MSFT_Disk lookup by disk number == MSFT_PhysicalDisk
// DeviceId (they line up on standard configurations). Any failure (namespace or
// class absent, e.g. very old Windows) yields an empty list rather than an error
// so check_disk_health still reports space + I/O.
devices_type query() {
  devices_type devices;
  std::map<long long, std::pair<bool, bool> > disk_flags;  // number -> (offline, readonly)
  try {
    wmi_impl::query dq("select Number, IsOffline, IsReadOnly from MSFT_Disk", "root\\Microsoft\\Windows\\Storage", "", "");
    wmi_impl::row_enumerator drow = dq.execute();
    while (drow.has_next()) {
      const wmi_impl::row r = drow.get_next();
      disk_flags[r.get_int("Number")] = std::make_pair(r.get_int("IsOffline") != 0, r.get_int("IsReadOnly") != 0);
    }
  } catch (const wmi_impl::wmi_exception &) {
    // MSFT_Disk unavailable: leave the offline/readonly flags at their defaults.
  }
  try {
    wmi_impl::query pq("select DeviceId, FriendlyName, SerialNumber, MediaType, HealthStatus from MSFT_PhysicalDisk", "root\\Microsoft\\Windows\\Storage", "",
                       "");
    wmi_impl::row_enumerator prow = pq.execute();
    while (prow.has_next()) {
      const wmi_impl::row r = prow.get_next();
      disk_device d;
      d.number = r.get_int("DeviceId");
      d.friendly_name = r.get_string("FriendlyName");
      d.serial = r.get_string("SerialNumber");
      d.media_type = disk_device::map_media_type(r.get_int("MediaType"));
      d.health_status = disk_device::map_health_status(r.get_int("HealthStatus"));
      const auto it = disk_flags.find(d.number);
      if (it != disk_flags.end()) {
        d.is_offline = it->second.first;
        d.is_readonly = it->second.second;
      }
      devices.push_back(d);
    }
  } catch (const wmi_impl::wmi_exception &) {
    // MSFT_PhysicalDisk / the Storage namespace is unavailable: no device rows.
  }
  return devices;
}

}  // namespace disk_device_check

namespace disk_free_check {

bool disk_free_data::fetch() {
  drives_type tmp;

  char buf[512];
  const DWORD len = GetLogicalDriveStringsA(sizeof(buf) - 1, buf);
  if (len == 0) return false;

  for (const char *p = buf; *p; p += strlen(p) + 1) {
    std::string drive(p);
    const UINT type = GetDriveTypeA(drive.c_str());
    if (type != DRIVE_FIXED && type != DRIVE_REMOTE && type != DRIVE_RAMDISK) continue;

    ULARGE_INTEGER free_avail, total_bytes, total_free;
    if (!GetDiskFreeSpaceExA(drive.c_str(), &free_avail, &total_bytes, &total_free)) continue;

    disk_free d;
    // Strip trailing backslash: "C:\" -> "C:"
    d.name = drive;
    if (!d.name.empty() && d.name.back() == '\\') d.name.pop_back();
    d.total = static_cast<long long>(total_bytes.QuadPart);
    d.free = static_cast<long long>(total_free.QuadPart);
    d.user_free = static_cast<long long>(free_avail.QuadPart);
    tmp.push_back(d);
  }

  set(tmp);
  return true;
}

}  // namespace disk_free_check
