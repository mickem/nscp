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
