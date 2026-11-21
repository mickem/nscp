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

// ---------------------------------------------------------------------------
// Line-break injection: a free-text field from the OS must never end its line
// early, or the rest poses as a row or a `<<<section>>>` header of its own.
// ---------------------------------------------------------------------------

TEST(CheckMkFormat, SanitizeFieldBlanksEveryControlCharacter) {
  EXPECT_EQ("a b", sanitize_field("a\nb"));
  EXPECT_EQ("a b", sanitize_field("a\rb"));
  EXPECT_EQ("a  b", sanitize_field("a\r\nb"));
  const std::string in = std::string("a") + '\0' + "b\tc\x01" + "d\x1f" + "e\x7f" + "f";
  EXPECT_EQ("a b c d e f", sanitize_field(in));
  for (int c = 0; c < 0x20; c++) EXPECT_EQ(" ", sanitize_field(std::string(1, static_cast<char>(c)))) << "control character " << c;
}

TEST(CheckMkFormat, SanitizeFieldKeepsSpacesAndUtf8Exactly) {
  EXPECT_EQ("a  b   c ", sanitize_field("a  b   c "));
  const std::string utf8 = "C:\\Program Files\\\xc3\xa5\xc3\xa4\xc3\xb6\\app.exe";
  EXPECT_EQ(utf8, sanitize_field(utf8));
}

TEST(CheckMkFormat, PsCommandLineCannotForgeASection) {
  // Anyone who can start a process chooses its command line; on Linux the
  // arguments in /proc/<pid>/cmdline may contain newlines.
  const std::string line = ps_line("u", 10, 20, 30, 1234, "sleep 100\n<<<fake>>>\nforged row");
  EXPECT_EQ("(u,10,20,30,1234) sleep 100 <<<fake>>> forged row", line);
  EXPECT_EQ(std::string::npos, line.find('\n'));
  EXPECT_EQ("(u,1,2,3,4) x <<<fake>>>", ps_line("u", 1, 2, 3, 4, "x\r<<<fake>>>"));
}

TEST(CheckMkFormat, PsCommandLineKeepsItsSpacing) { EXPECT_EQ("(u,1,2,3,4) a  b   c", ps_line("u", 1, 2, 3, 4, "a  b   c")); }

TEST(CheckMkFormat, PsCommandLineStartingWithALineBreakKeepsTheProgramFirst) {
  EXPECT_EQ("(u,1,2,3,4) <<<fake>>>", ps_line("u", 1, 2, 3, 4, "\n<<<fake>>>"));
  EXPECT_EQ("(u,1,2,3,4) [unknown]", ps_line("u", 1, 2, 3, 4, "\r\n"));
}

TEST(CheckMkFormat, PsUserCannotBreakTheLine) { EXPECT_EQ("(ev_il,1,2,3,4) x", ps_line("ev\nil", 1, 2, 3, 4, "x")); }

TEST(CheckMkFormat, DfMountpointCannotForgeASection) {
  const std::string line = df_line("/dev/sdb1", "vfat", 10, 5, 5, "/media/usb\n<<<fake>>>\n/dev/sda1 ext4 1 1 0 100% /");
  EXPECT_EQ(std::string::npos, line.find('\n'));
  EXPECT_EQ("/dev/sdb1 vfat 10 5 5 50% /media/usb <<<fake>>> /dev/sda1 ext4 1 1 0 100% /", line);
  const std::string inodes = df_inodes_line("/dev/sdb1", "ext4", 100, 50, "/m\r\n<<<fake>>>");
  EXPECT_EQ(std::string::npos, inodes.find('\n'));
  EXPECT_EQ(std::string::npos, inodes.find('\r'));
}

TEST(CheckMkFormat, DfTokensCannotBreakTheLine) { EXPECT_EQ("/dev/x fuse_y 1 1 0 100% /", df_line("/dev/x", "fuse\ny", 1, 1, 0, "/")); }

TEST(CheckMkFormat, ServiceDisplayNameCannotForgeASection) {
  const std::string line = service_line("evil", "running", "auto", "Nice  name\r\n<<<fake>>>\r\nforged");
  EXPECT_EQ(std::string::npos, line.find('\n'));
  EXPECT_EQ(std::string::npos, line.find('\r'));
  EXPECT_EQ("evil running/auto Nice  name  <<<fake>>>  forged", line);
}

TEST(CheckMkFormat, ServiceNameStandingInForTheDisplayNameCannotBreakTheLine) {
  EXPECT_EQ("a_b stopped/demand a b", service_line("a\nb", "stopped", "demand", ""));
}

TEST(CheckMkFormat, SanitizeTokenKeepsTheTokenWhole) {
  // A control character in a token column must not become a space: the
  // columns after it would shift.
  EXPECT_EQ("a_b", sanitize_token("a\nb"));
  EXPECT_EQ("a__b", sanitize_token("a\r\nb"));
  EXPECT_EQ("a_b", sanitize_token("a\tb"));
  EXPECT_EQ("a b", sanitize_token("a b"));
  const std::string utf8 = "caf\xc3\xa9.service";
  EXPECT_EQ(utf8, sanitize_token(utf8));
}
