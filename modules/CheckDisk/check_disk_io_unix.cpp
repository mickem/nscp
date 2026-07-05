// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unix data acquisition for the disk I/O / disk free subsystem. Disk I/O rates
// are derived from /proc/diskstats cumulative counters sampled over time; free
// space is read with statvfs over the real mounts. The metric builders / check
// logic are shared in check_disk_io.cpp.

#include "check_disk_io.hpp"

#include <dirent.h>
#include <mntent.h>
#include <sys/statvfs.h>

#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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

// Strip a trailing partition suffix to get the parent device name ("sda1" ->
// "sda", "nvme0n1p2" -> "nvme0n1", "md0p2" -> "md0"). Names without a
// recognized suffix are returned unchanged.
std::string strip_partition_suffix(const std::string &n) {
  std::size_t i = n.size();
  while (i > 0 && std::isdigit(static_cast<unsigned char>(n[i - 1]))) i--;
  if (i == n.size() || i == 0) return n;  // no trailing digit -> already a whole disk
  // "pN" suffix on devices whose base name itself ends in a digit (nvme0n1p2,
  // mmcblk0p1, md0p2).
  if (n[i - 1] == 'p' && i > 1 && std::isdigit(static_cast<unsigned char>(n[i - 2]))) return n.substr(0, i - 1);
  if (n.compare(0, 2, "sd") == 0 || n.compare(0, 2, "vd") == 0 || n.compare(0, 2, "hd") == 0 || n.compare(0, 3, "xvd") == 0) {
    return n.substr(0, i);  // sda1 -> sda
  }
  return n;
}

// realpath() wrapper: canonicalize /dev/disk/by-uuid/... and /dev/mapper/...
// symlinks to the real block node (e.g. /dev/dm-0). Returns "" on failure.
std::string canonicalize_dev(const std::string &path) {
  char buf[PATH_MAX];
  if (realpath(path.c_str(), buf) == nullptr) return "";
  return buf;
}

// Entries under /sys/class/block/<name>/slaves: the devices a stacked block
// device (LVM/dm, md, ...) sits on top of. Empty for plain disks/partitions.
std::vector<std::string> sysfs_slaves(const std::string &name) {
  std::vector<std::string> ret;
  DIR *dir = opendir(("/sys/class/block/" + name + "/slaves").c_str());
  if (!dir) return ret;
  while (const struct dirent *e = readdir(dir)) {
    const std::string entry = e->d_name;
    if (entry != "." && entry != "..") ret.push_back(entry);
  }
  closedir(dir);
  return ret;
}

// Map a /proc/mounts device path (mnt_fsname, e.g. "/dev/sda1",
// "/dev/mapper/vg-root", "/dev/disk/by-uuid/...") to the whole physical block
// device name used as the key in /proc/diskstats (e.g. "sda", "nvme0n1").
// `canonicalize` resolves symlinks (realpath) and `slaves` lists
// /sys/class/block/<name>/slaves; both are injectable for unit tests. Returns
// "" for anything that cannot be resolved to a physical disk (network mounts,
// tmpfs, ...), in which case the disk_health join keeps the row space-only.
std::string mount_source_to_disk(const std::string &fsname, const std::function<std::string(const std::string &)> &canonicalize,
                                 const std::function<std::vector<std::string>(const std::string &)> &slaves) {
  const std::string prefix = "/dev/";
  if (fsname.compare(0, prefix.size(), prefix) != 0) return "";
  std::string path = canonicalize(fsname);
  if (path.empty()) path = fsname;
  if (path.compare(0, prefix.size(), prefix) != 0) return "";
  const std::string base = path.substr(prefix.size());
  if (base.find('/') != std::string::npos) return "";  // unresolved /dev subdirectory entry
  // Walk stacked devices (LVM on LUKS on md, ...) down to the physical disk
  // they sit on; bounded in case of sysfs cycles. A multi-disk array resolves
  // to its (lexicographically) first member; the other members are left as
  // I/O-only rows in the disk_health join.
  std::string n = base;
  for (int depth = 0; depth < 8; ++depth) {
    n = strip_partition_suffix(n);
    const std::vector<std::string> s = slaves(n);
    if (s.empty()) break;
    n = *std::min_element(s.begin(), s.end());
  }
  return is_physical_disk(n) ? n : "";
}

std::string device_to_disk(const std::string &fsname) { return mount_source_to_disk(fsname, canonicalize_dev, sysfs_slaves); }

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
    d.device = device_to_disk(ent.mnt_fsname ? ent.mnt_fsname : "");
    d.total = static_cast<long long>(vfs.f_blocks * bs);
    d.free = static_cast<long long>(vfs.f_bfree * bs);
    d.user_free = static_cast<long long>(vfs.f_bavail * bs);
    tmp.push_back(d);
  }
  endmntent(fp);
  set(tmp);
}

}  // namespace disk_free_check

// Test seam: resolve a mount source to its backing physical disk with
// injectable symlink/sysfs lookups (no syscalls, so it is directly
// unit-testable).
std::string checkdisk_unix_mount_source_to_disk(const std::string &fsname, const std::function<std::string(const std::string &)> &canonicalize,
                                                const std::function<std::vector<std::string>(const std::string &)> &slaves) {
  return mount_source_to_disk(fsname, canonicalize, slaves);
}
