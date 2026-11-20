// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <check/check_mk_format.hpp>

using namespace check_mk_format;

TEST(CheckMkFormat, PsLineCarriesTheOwner) { EXPECT_EQ("(root,1024,512,7,1) /sbin/init splash", ps_line("root", 1024, 512, 7, 1, "/sbin/init splash")); }

TEST(CheckMkFormat, PsLineWithUnknownOwnerKeepsTheFieldCount) { EXPECT_EQ("(,10,20,0,42) a.exe", ps_line("", 10, 20, 0, 42, "a.exe")); }

TEST(CheckMkFormat, PsLineAlwaysHasACommandColumn) {
  EXPECT_EQ("(,0,0,0,4) [unknown]", ps_line("", 0, 0, 0, 4, ""));
  EXPECT_EQ("(,0,0,0,4) [unknown]", ps_line("", 0, 0, 0, 4, "  "));
}

TEST(CheckMkFormat, PsLineKeepsTheProgramFirst) { EXPECT_EQ("(u,1,2,0,9) --vm-id {x}", ps_line("u", 1, 2, 0, 9, "  --vm-id {x}")); }

TEST(CheckMkFormat, PsUserCannotSplitTheProcessInfoToken) {
  // A space would end the token, a comma would shift vsz/rss/cputime/pid.
  EXPECT_EQ("NT_AUTHORITY\\SYSTEM", ps_user("NT AUTHORITY\\SYSTEM"));
  EXPECT_EQ("Smith_John", ps_user("Smith,John"));
  EXPECT_EQ("a_b_", ps_user("a(b)"));
  EXPECT_EQ("tab_user", ps_user("tab\tuser"));
}

TEST(CheckMkFormat, DfLineMatchesDfPosixLayout) {
  // The numbers of a real `df -PTk /` line: 41% is ceil(used / (used + avail)).
  EXPECT_EQ("/dev/sda1 ext4 30297152 11962620 17371776 41% /", df_line("/dev/sda1", "ext4", 30297152, 11962620, 17371776, "/"));
}

TEST(CheckMkFormat, DfLineKeepsSpacesInTheMountpointOnly) {
  EXPECT_EQ("my_dev unknown 100 50 50 50% /mnt/my disk", df_line("my dev", "", 100, 50, 50, "/mnt/my disk"));
}

TEST(CheckMkFormat, DfLineWithoutDeviceStillHasSevenColumns) { EXPECT_EQ("none tmpfs 0 0 0 - /run", df_line("", "tmpfs", 0, 0, 0, "/run")); }

TEST(CheckMkFormat, DfPercentRoundsUpLikeDf) {
  EXPECT_EQ("1%", df_percent(1, 999));
  EXPECT_EQ("100%", df_percent(10, 0));
  EXPECT_EQ("0%", df_percent(0, 10));
  EXPECT_EQ("-", df_percent(0, 0));
}

TEST(CheckMkFormat, DfInodesLineMatchesDfInodeLayout) {
  EXPECT_EQ("/dev/sda1 ext4 1921360 391218 1530142 21% /", df_inodes_line("/dev/sda1", "ext4", 1921360, 1530142, "/"));
}

TEST(CheckMkFormat, ServiceStartTypeFoldsEveryAutoStartVariantIntoAuto) {
  // Checkmk's Service Summary counts only `auto` as autostart.
  EXPECT_EQ("auto", service_start_type("auto"));
  EXPECT_EQ("auto", service_start_type("delayed"));
  EXPECT_EQ("auto", service_start_type("auto_trigger"));
  EXPECT_EQ("auto", service_start_type("delayed_trigger"));
  EXPECT_EQ("demand", service_start_type("demand"));
  EXPECT_EQ("disabled", service_start_type("disabled"));
  EXPECT_EQ("unknown", service_start_type("unknown"));
}

TEST(CheckMkFormat, ServiceLineMatchesTheWindowsAgent) {
  EXPECT_EQ("AudioSrv running/auto Windows Audio", service_line("AudioSrv", "running", "auto", "Windows Audio"));
  EXPECT_EQ("MapsBroker stopped/auto Downloaded Maps Manager", service_line("MapsBroker", "stopped", "delayed", "Downloaded Maps Manager"));
}

TEST(CheckMkFormat, ServiceNameWithSpacesStaysOneColumn) {
  // Checkmk's own sample: `Bonjour_Service running/auto Dienst "Bonjour"`.
  EXPECT_EQ("Bonjour_Service running/auto Dienst \"Bonjour\"", service_line("Bonjour Service", "running", "auto", "Dienst \"Bonjour\""));
  EXPECT_EQ("Windows_Agent_Service stopped/auto Windows Agent Service", service_line("Windows Agent Service", "stopped", "auto", ""));
}

TEST(CheckMkFormat, DfInodesLineWithoutInodeTablePrintsDash) {
  EXPECT_EQ("/dev/sdb1 vfat 0 0 0 - /boot/efi", df_inodes_line("/dev/sdb1", "vfat", 0, 0, "/boot/efi"));
}
