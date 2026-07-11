// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_share.hpp"

#include <gtest/gtest.h>

// nscapi::plugin_singleton is defined once in check_disk_io_test.cpp for the
// merged check_disk_test target.

using share_check::share_info;

TEST(Share, TypeMapping) {
  EXPECT_EQ(share_info::type_to_string(0), "disk");
  EXPECT_EQ(share_info::type_to_string(1), "printer");
  EXPECT_EQ(share_info::type_to_string(2), "device");
  EXPECT_EQ(share_info::type_to_string(3), "ipc");
  EXPECT_EQ(share_info::type_to_string(99), "unknown");
}

TEST(Share, AdminFlagFromHighBit) {
  share_info disk;
  disk.type_raw = 0;  // ordinary disk share
  EXPECT_EQ(disk.get_is_admin(), 0);
  EXPECT_EQ(disk.get_type(), "disk");

  share_info admin;
  admin.type_raw = 0x80000000;  // administrative disk share (e.g. C$)
  EXPECT_EQ(admin.get_is_admin(), 1);
  EXPECT_EQ(admin.get_type(), "disk");

  share_info ipcAdmin;
  ipcAdmin.type_raw = 0x80000003;  // IPC$ (admin + ipc)
  EXPECT_EQ(ipcAdmin.get_is_admin(), 1);
  EXPECT_EQ(ipcAdmin.get_type(), "ipc");
}

TEST(Share, ExistsFlag) {
  share_info present;
  present.exists = true;
  EXPECT_EQ(present.get_exists(), 1);

  share_info missing;  // default-constructed: not present
  EXPECT_EQ(missing.get_exists(), 0);
}

TEST(Share, Accessors) {
  share_info s;
  s.name = "Public";
  s.path = "C:\\Shared";
  s.description = "Public files";
  EXPECT_EQ(s.get_name(), "Public");
  EXPECT_EQ(s.get_path(), "C:\\Shared");
  EXPECT_EQ(s.get_description(), "Public files");
  EXPECT_EQ(s.show(), "Public");
}
