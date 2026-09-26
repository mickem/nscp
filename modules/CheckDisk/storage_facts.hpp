// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

// The `storage` fact set: which volumes this host has, what they are and how
// big they are.
//
// The inventory, not the monitoring: a volume's size is here, its free space
// is not. Free space moves every minute, and a value that moves every round
// would bump the document's revision every round and make the fleet re-upload
// the whole inventory each hour for no new information. check_drivesize is
// where free space lives.
//
// The split mirrors host_facts: the platform-specific enumeration sits next
// to the check that already does it (check_drive_win.cpp / check_drive_linux.cpp,
// whose helpers are private to those files), and everything that decides what
// a record looks like is here, shared, so a Windows volume and a Linux mount
// are the same shape in a mixed fleet.
namespace storage_facts {

// The fact set this module produces, and the one list in it. The enableable
// id is the dotted path, `storage.volumes`: that is the settings key, and what
// the producer names when it claims the set.
extern const char *const set_storage;
extern const char *const key_volumes;
extern const char *const id_volumes;

// One volume, as a platform's gather reads it. Empty strings and a zero size
// mean "not known" and are omitted from the record, never written empty.
struct volume {
  // The record id, and the one field a later "context on failure" feature
  // hangs on: exactly the `drive` keyword of check_drivesize for the same
  // volume. On Windows the path the volume is mounted on (`C:\`, or a folder
  // for a volume mounted into one); on unix the mount point (`/`, `/home`).
  std::string id;
  // What backs it: the block device on unix (`/dev/sda1`), the volume GUID
  // path on Windows (`\\?\Volume{…}\`).
  std::string device;
  std::string filesystem;  // `ext4`, `xfs`, `NTFS`, `ReFS`, …
  // The check_drivesize `type` vocabulary: fixed, remote, removable, cdrom,
  // ramdisk, unknown.
  std::string type;
  std::string label;  // the filesystem label, where it has one
  unsigned long long size_bytes = 0;
};

// Add the `storage` set, with every volume in `volumes` as a record of its
// `volumes` list, to `out`. `taken_at` stamps when the values were read.
void publish(const std::vector<volume> &volumes, std::time_t taken_at, nscapi::facts::response &out);

// udev escapes the characters it cannot put in a file name when it names the
// symlinks under /dev/disk/by-label, so a label with a space in it arrives as
// `My\x20Disk`. Decode the `\xNN` escapes back into the label; anything that
// is not a well-formed escape is kept as it is. Pure, and platform-neutral so
// it is unit-tested on every build.
std::string decode_udev_label(const std::string &escaped);

// Read this host's volumes. Implemented per platform, in check_drive_win.cpp
// and check_drive_linux.cpp, where the enumeration check_drivesize already uses
// lives.
//
// Never touches a remote volume for its size: an hourly background round has
// no business hanging on a dead NFS server or a disconnected share, which is
// exactly what statvfs / GetDiskFreeSpaceEx do there. A remote volume is
// listed, with its type and device, and without `size_bytes`.
std::vector<volume> gather();

}  // namespace storage_facts
