// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts_volumes.hpp"

#include <mntent.h>
#include <stdio.h>
#include <sys/statvfs.h>

#include <set>
#include <string>
#include <vector>

namespace check_disk_facts {

namespace {

// The same classification `check_drivesize` applies, so the `type` on a
// record and the `type` keyword on the check agree about what a volume is.
std::string classify(const std::string &fstype) {
  static const std::set<std::string> remote = {"nfs", "nfs4", "cifs", "smbfs", "smb3", "ncpfs", "afs", "9p", "fuse.sshfs", "glusterfs", "ceph", "beegfs"};
  static const std::set<std::string> ramdisk = {"tmpfs", "ramfs", "devtmpfs"};
  static const std::set<std::string> cdrom = {"iso9660", "udf"};
  if (remote.count(fstype)) return "remote";
  if (ramdisk.count(fstype)) return "ramdisk";
  if (cdrom.count(fstype)) return "cdrom";
  return "fixed";
}

// Kernel bookkeeping rather than storage: publishing it would bury the half
// dozen filesystems an operator actually has in fifty that every Linux host
// has. The list is the check's, for the same reason.
bool is_pseudo(const std::string &fstype) {
  static const std::set<std::string> pseudo = {"proc",
                                               "sysfs",
                                               "cgroup",
                                               "cgroup2",
                                               "devtmpfs",
                                               "devpts",
                                               "mqueue",
                                               "hugetlbfs",
                                               "debugfs",
                                               "tracefs",
                                               "securityfs",
                                               "pstore",
                                               "bpf",
                                               "configfs",
                                               "fusectl",
                                               "autofs",
                                               "binfmt_misc",
                                               "rpc_pipefs",
                                               "nsfs",
                                               "efivarfs",
                                               "ramfs",
                                               "selinuxfs",
                                               "fuse.gvfsd-fuse",
                                               "fuse.portal",
                                               "overlay",
                                               "tmpfs",
                                               "sysvfs",
                                               "squashfs",
                                               "fuse.snapfuse"};
  return pseudo.count(fstype) > 0;
}

}  // namespace

std::vector<volume> gather_volumes(std::string &error) {
  std::vector<volume> volumes;
  FILE *table = setmntent("/proc/mounts", "r");
  if (table == nullptr) table = setmntent("/etc/mtab", "r");
  if (table == nullptr) {
    error = "could not read the mount table (/proc/mounts, /etc/mtab)";
    return volumes;
  }

  std::set<std::string> seen;
  struct mntent entry;
  char buffer[4096];
  while (getmntent_r(table, &entry, buffer, sizeof(buffer)) != nullptr) {
    const std::string fstype = entry.mnt_type ? entry.mnt_type : "";
    const std::string mount = entry.mnt_dir ? entry.mnt_dir : "";
    if (mount.empty() || is_pseudo(fstype)) continue;
    // A bind mount appears twice in /proc/mounts; the record id has to be
    // unique or the core rejects the whole set.
    if (!seen.insert(mount).second) continue;

    volume found;
    found.id = mount;
    found.device = entry.mnt_fsname ? entry.mnt_fsname : "";
    found.fs = fstype;
    found.type = classify(fstype);

    struct statvfs stats;
    if (statvfs(mount.c_str(), &stats) == 0 && stats.f_frsize > 0) {
      found.size_bytes = static_cast<unsigned long long>(stats.f_blocks) * static_cast<unsigned long long>(stats.f_frsize);
      found.has_size = true;
    }
    volumes.push_back(found);
  }
  endmntent(table);
  return volumes;
}

}  // namespace check_disk_facts
