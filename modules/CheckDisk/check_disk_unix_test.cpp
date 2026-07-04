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

#include <gtest/gtest.h>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <functional>
#include <map>
#include <string>
#include <vector>

// The filter framework logs through the plugin singleton; the unit test links
// those objects without a module wrapper, so define it here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

// Test seam exported by check_drive_unix.cpp: maps a filesystem type string to
// the drive-type keyword used by the `type` filter field (no syscalls, so it is
// directly unit-testable).
extern std::string checkdisk_unix_classify_fs(const std::string &fstype);

TEST(CheckDiskUnixClassify, NetworkFilesystemsAreRemote) {
  EXPECT_EQ(checkdisk_unix_classify_fs("nfs"), "remote");
  EXPECT_EQ(checkdisk_unix_classify_fs("nfs4"), "remote");
  EXPECT_EQ(checkdisk_unix_classify_fs("cifs"), "remote");
  EXPECT_EQ(checkdisk_unix_classify_fs("9p"), "remote");
}

TEST(CheckDiskUnixClassify, MemoryFilesystemsAreRamdisk) {
  EXPECT_EQ(checkdisk_unix_classify_fs("tmpfs"), "ramdisk");
  EXPECT_EQ(checkdisk_unix_classify_fs("ramfs"), "ramdisk");
  EXPECT_EQ(checkdisk_unix_classify_fs("devtmpfs"), "ramdisk");
}

TEST(CheckDiskUnixClassify, OpticalFilesystemsAreCdrom) {
  EXPECT_EQ(checkdisk_unix_classify_fs("iso9660"), "cdrom");
  EXPECT_EQ(checkdisk_unix_classify_fs("udf"), "cdrom");
}

TEST(CheckDiskUnixClassify, RealDisksAreFixed) {
  EXPECT_EQ(checkdisk_unix_classify_fs("ext4"), "fixed");
  EXPECT_EQ(checkdisk_unix_classify_fs("xfs"), "fixed");
  EXPECT_EQ(checkdisk_unix_classify_fs("btrfs"), "fixed");
  EXPECT_EQ(checkdisk_unix_classify_fs("vfat"), "fixed");
}

// Test seam exported by check_disk_io_unix.cpp: resolves a /proc/mounts device
// path to the whole physical disk it lives on, with the symlink (realpath) and
// sysfs slaves lookups injected so no real /dev or /sys is needed.
extern std::string checkdisk_unix_mount_source_to_disk(const std::string &fsname, const std::function<std::string(const std::string &)> &canonicalize,
                                                       const std::function<std::vector<std::string>(const std::string &)> &slaves);

namespace {

// Fake realpath: resolves the given symlinks, leaves other paths unchanged.
std::function<std::string(const std::string &)> fake_links(const std::map<std::string, std::string> &links = {}) {
  return [links](const std::string &path) {
    const auto it = links.find(path);
    return it == links.end() ? path : it->second;
  };
}

// Fake /sys/class/block/<name>/slaves.
std::function<std::vector<std::string>(const std::string &)> fake_slaves(const std::map<std::string, std::vector<std::string>> &slaves = {}) {
  return [slaves](const std::string &name) {
    const auto it = slaves.find(name);
    return it == slaves.end() ? std::vector<std::string>() : it->second;
  };
}

}  // namespace

TEST(CheckDiskUnixMountSource, PlainPartitionsStripToWholeDisk) {
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/sda1", fake_links(), fake_slaves()), "sda");
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/sda", fake_links(), fake_slaves()), "sda");
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/nvme0n1p2", fake_links(), fake_slaves()), "nvme0n1");
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/mmcblk0p1", fake_links(), fake_slaves()), "mmcblk0");
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/xvda1", fake_links(), fake_slaves()), "xvda");
}

TEST(CheckDiskUnixMountSource, ByUuidSymlinkIsCanonicalized) {
  const auto links = fake_links({{"/dev/disk/by-uuid/1234-ABCD", "/dev/sda2"}});
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/disk/by-uuid/1234-ABCD", links, fake_slaves()), "sda");
}

TEST(CheckDiskUnixMountSource, LvmMapperResolvesThroughDmSlaves) {
  // Stock LVM install: / on /dev/mapper/vg-root (dm-0) backed by sda2.
  const auto links = fake_links({{"/dev/mapper/vg-root", "/dev/dm-0"}});
  const auto slaves = fake_slaves({{"dm-0", {"sda2"}}});
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/mapper/vg-root", links, slaves), "sda");
}

TEST(CheckDiskUnixMountSource, StackedDmLayersAreWalkedRecursively) {
  // LVM on LUKS: dm-1 (LV) on dm-0 (crypt) on nvme0n1p3.
  const auto links = fake_links({{"/dev/mapper/vg-root", "/dev/dm-1"}});
  const auto slaves = fake_slaves({{"dm-1", {"dm-0"}}, {"dm-0", {"nvme0n1p3"}}});
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/mapper/vg-root", links, slaves), "nvme0n1");
}

TEST(CheckDiskUnixMountSource, MdPartitionResolvesToFirstArrayMember) {
  // Partitioned RAID (md0p2): strip to md0, then walk to its first member.
  const auto slaves = fake_slaves({{"md0", {"sdb1", "sda1"}}});
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/md0p2", fake_links(), slaves), "sda");
}

TEST(CheckDiskUnixMountSource, UnresolvableSourcesReturnEmpty) {
  // Not /dev at all (network mounts, tmpfs, zfs datasets).
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("tmpfs", fake_links(), fake_slaves()), "");
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("server:/export", fake_links(), fake_slaves()), "");
  // Mapper node whose dm device has no slaves does not fake a physical disk.
  const auto links = fake_links({{"/dev/mapper/foo", "/dev/dm-3"}});
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/mapper/foo", links, fake_slaves()), "");
  // Unresolvable /dev subdirectory entry (realpath failed).
  const auto broken = [](const std::string &) { return std::string(); };
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/mapper/vg-root", broken, fake_slaves()), "");
}

TEST(CheckDiskUnixMountSource, RealpathFailureFallsBackToLiteralPath) {
  const auto broken = [](const std::string &) { return std::string(); };
  EXPECT_EQ(checkdisk_unix_mount_source_to_disk("/dev/sda1", broken, fake_slaves()), "sda");
}
