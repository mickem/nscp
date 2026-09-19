// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

// The data source behind the `storage.volumes` fact set: one record per
// mounted filesystem, the same volumes `check_drivesize` reports on.
//
// The check itself enumerates drives inside check_drive_win.cpp /
// check_drive_unix.cpp, in service of a filter expression and a threshold;
// what inventory needs is smaller and has no filter to satisfy, so it gets its
// own gatherer rather than a second caller of that machinery. What the two do
// share is the identity: `volume::id` is the same string the check's `drive`
// keyword produces (the mount point on Unix, the drive letter on Windows), so
// a failing `check_drivesize` on `/var` and the `storage.volumes` record for
// `/var` name the same thing - that convention is what a later "context on a
// failing check" feature is built on, and the unit tests hold it.
namespace check_disk_facts {

struct volume {
  // Mount point (Unix) or drive letter with its colon (Windows). Stable
  // across runs, which is what lets the fleet server diff the list.
  std::string id;
  // The backing device or share, where the platform reports one.
  std::string device;
  // Filesystem as the OS names it: `ext4`, `NTFS`, `vfat`, …
  std::string fs;
  // `fixed`, `removable`, `remote`, `cdrom`, `ramdisk` or `unknown` - the
  // same vocabulary the check's `type` keyword uses, and the same words on
  // both platforms, so a fleet query for removable media does not have to
  // know which OS answered.
  std::string type;
  // Volume label, where the platform has one (Windows).
  std::string label;
  unsigned long long size_bytes = 0;
  // False when the size could not be read (an unreadable or empty removable
  // drive). The record is still published - the volume exists - but without a
  // size, because 0 would be a claim the host never made.
  bool has_size = false;
};

// Every mounted filesystem worth putting in an inventory. Pseudo filesystems
// (proc, sysfs, cgroup, tmpfs, overlay, …) are left out: they are properties
// of the kernel, not of the machine, and they would dominate the list.
//
// Returns what it could read. `error` is set when the enumeration itself
// failed, which the core reports on the fact set rather than blanking it.
std::vector<volume> gather_volumes(std::string &error);

}  // namespace check_disk_facts
