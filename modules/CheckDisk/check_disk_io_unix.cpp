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

// Unix data acquisition for the disk I/O / disk free subsystem. Disk I/O rates
// are derived from /proc/diskstats cumulative counters sampled over time; free
// space is read with statvfs over the real mounts. The metric builders / check
// logic are shared in check_disk_io.cpp.

#include "check_disk_io.hpp"

#include <mntent.h>
#include <sys/statvfs.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <cctype>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace {

constexpr long long SECTOR_BYTES = 512;

long long now_ms() {
  static const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  return (boost::posix_time::microsec_clock::universal_time() - epoch).total_milliseconds();
}

bool is_partition(const std::string &n) {
  std::size_t i = n.size();
  while (i > 0 && std::isdigit(static_cast<unsigned char>(n[i - 1]))) i--;
  if (i == n.size()) return false;  // no trailing digit -> whole disk (e.g. sda)
  if (i > 0 && n[i - 1] == 'p' && (n.compare(0, 4, "nvme") == 0 || n.compare(0, 6, "mmcblk") == 0)) return true;
  if (i > 0 && std::isalpha(static_cast<unsigned char>(n[i - 1]))) {
    if (n.compare(0, 2, "sd") == 0 || n.compare(0, 2, "vd") == 0 || n.compare(0, 2, "hd") == 0 || n.compare(0, 3, "xvd") == 0) return true;
  }
  return false;
}

// Keep only whole physical block devices; skip partitions and virtual devices.
bool is_physical_disk(const std::string &n) {
  static const char *skip[] = {"loop", "ram", "dm-", "sr", "fd", "zram", "md"};
  for (const char *s : skip) {
    if (n.compare(0, std::strlen(s), s) == 0) return false;
  }
  return !is_partition(n);
}

bool is_pseudo_fs(const std::string &fstype) {
  static const std::set<std::string> pseudo = {"proc",     "sysfs",      "cgroup",   "cgroup2",  "devtmpfs",  "devpts",   "mqueue",
                                               "hugetlbfs", "debugfs",    "tracefs",  "securityfs", "pstore",  "bpf",      "configfs",
                                               "fusectl",   "autofs",     "binfmt_misc", "rpc_pipefs", "nsfs",  "efivarfs", "ramfs",
                                               "selinuxfs", "overlay",    "tmpfs",    "squashfs", "fuse.snapfuse"};
  return pseudo.count(fstype) > 0;
}

}  // namespace

namespace disk_io_check {

void disk_io_data::fetch() {
  std::ifstream f("/proc/diskstats");
  if (!f) return;

  const long long t = now_ms();
  const double dt = prev_time_ms_ > 0 ? (t - prev_time_ms_) / 1000.0 : 0.0;

  std::map<std::string, std::vector<unsigned long long>> current;
  disks_type disks;
  disk_io total;
  total.name = "_Total";

  std::string line;
  while (std::getline(f, line)) {
    std::istringstream ss(line);
    unsigned long long major, minor;
    std::string name;
    if (!(ss >> major >> minor >> name)) continue;
    if (!is_physical_disk(name)) continue;

    unsigned long long v[11] = {0};
    for (int i = 0; i < 11 && (ss >> v[i]); ++i) {
    }
    // v[0]=reads v[2]=sectors_read v[4]=writes v[6]=sectors_written v[8]=in_flight v[9]=ms_io
    const std::vector<unsigned long long> raw = {v[0], v[2], v[4], v[6], v[9]};
    current[name] = raw;

    disk_io d;
    d.name = name;
    d.queue_length = static_cast<long long>(v[8]);

    auto it = prev_raw_.find(name);
    if (dt > 0 && it != prev_raw_.end() && it->second.size() == 5) {
      const auto &p = it->second;
      auto delta = [](unsigned long long cur, unsigned long long old) { return cur >= old ? cur - old : 0ull; };
      d.reads_per_sec = static_cast<long long>(delta(raw[0], p[0]) / dt);
      d.read_bytes_per_sec = static_cast<long long>(delta(raw[1], p[1]) * SECTOR_BYTES / dt);
      d.writes_per_sec = static_cast<long long>(delta(raw[2], p[2]) / dt);
      d.write_bytes_per_sec = static_cast<long long>(delta(raw[3], p[3]) * SECTOR_BYTES / dt);
      long long pct = static_cast<long long>((delta(raw[4], p[4]) / (dt * 1000.0)) * 100.0);
      if (pct > 100) pct = 100;
      d.percent_disk_time = pct;
      d.percent_idle_time = 100 - pct;
    } else {
      d.percent_idle_time = 100;
    }
    total.read_bytes_per_sec += d.read_bytes_per_sec;
    total.write_bytes_per_sec += d.write_bytes_per_sec;
    total.reads_per_sec += d.reads_per_sec;
    total.writes_per_sec += d.writes_per_sec;
    total.queue_length += d.queue_length;
    if (d.percent_disk_time > total.percent_disk_time) total.percent_disk_time = d.percent_disk_time;
    disks.push_back(d);
  }
  total.percent_idle_time = 100 - total.percent_disk_time;
  disks.push_back(total);

  prev_raw_.swap(current);
  prev_time_ms_ = t;
  set(disks);
}

}  // namespace disk_io_check

namespace disk_free_check {

void disk_free_data::fetch() {
  drives_type tmp;
  FILE *fp = setmntent("/proc/mounts", "r");
  if (!fp) return;
  struct mntent ent;
  char buf[4096];
  std::set<std::string> seen;
  while (getmntent_r(fp, &ent, buf, sizeof(buf)) != nullptr) {
    const std::string mountpoint = ent.mnt_dir ? ent.mnt_dir : "";
    const std::string fstype = ent.mnt_type ? ent.mnt_type : "";
    if (mountpoint.empty() || is_pseudo_fs(fstype)) continue;
    if (!seen.insert(mountpoint).second) continue;

    struct statvfs vfs;
    if (statvfs(mountpoint.c_str(), &vfs) != 0) continue;
    const unsigned long long bs = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    disk_free d;
    d.name = mountpoint;
    d.total = static_cast<long long>(vfs.f_blocks * bs);
    d.free = static_cast<long long>(vfs.f_bfree * bs);
    d.user_free = static_cast<long long>(vfs.f_bavail * bs);
    tmp.push_back(d);
  }
  endmntent(fp);
  set(tmp);
}

}  // namespace disk_free_check
