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
std::string helper::perf_namespace = "root\\CIMV2";

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

void disk_io_data::fetch() {
  if (!fetch_disk_io_) return;

  try {
    set(query_perf());
  } catch (const wmi_impl::wmi_exception &e) {
    if (e.get_code() == WBEM_E_INVALID_QUERY || e.get_code() == WBEM_E_NOT_FOUND) {
      fetch_disk_io_ = false;
      throw nsclient::nsclient_exception("Failed to fetch disk I/O metrics (performance counter not available), disabling...");
    }
    throw nsclient::nsclient_exception("Failed to fetch disk I/O metrics: " + e.reason());
  }
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

void disk_free_data::fetch() {
  drives_type tmp;

  char buf[512];
  const DWORD len = GetLogicalDriveStringsA(sizeof(buf) - 1, buf);
  if (len == 0) return;

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
}

}  // namespace disk_free_check
