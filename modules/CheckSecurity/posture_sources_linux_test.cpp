// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Linux reader behind check_secureboot, against fixtures: efivars byte
// layouts in a temporary directory tree. The command body itself is covered by
// check_posture_commands_test.cpp.

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check_secureboot.hpp"

namespace {

const std::string guid = "8be4df61-93ca-11d2-aa0d-00e098032b8c";

// A scratch directory removed at the end of the test.
class temp_dir {
 public:
  temp_dir() {
    char tmpl[] = "/tmp/nscp_posture_XXXXXX";
    const char *p = ::mkdtemp(tmpl);
    path_ = p ? p : "";
  }
  ~temp_dir() {
    if (!path_.empty()) std::system(("rm -rf '" + path_ + "'").c_str());
  }
  const std::string &path() const { return path_; }
  void mkdir(const std::string &rel) const { ::mkdir((path_ + "/" + rel).c_str(), 0755); }
  void write(const std::string &rel, const std::string &content, const mode_t mode = 0644) const {
    std::ofstream out(path_ + "/" + rel, std::ios::binary);
    out << content;
    out.close();
    ::chmod((path_ + "/" + rel).c_str(), mode);
  }

 private:
  std::string path_;
};

// An efivarfs file: 4 attribute bytes (NV|BS|RT = 0x07) then the value.
std::string efivar(const unsigned char value) { return std::string("\x07\x00\x00\x00", 4) + static_cast<char>(value); }

std::vector<secureboot_filter::filter_obj_ptr> read_secureboot(const std::string &efi_dir, std::string &error) {
  std::vector<secureboot_filter::filter_obj_ptr> out;
  secureboot_source::gather_from(efi_dir, out, error);
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// check_secureboot
// ---------------------------------------------------------------------------

TEST(secureboot_linux, the_value_is_the_byte_after_the_attributes) {
  long long value = -1;
  std::string error;
  ASSERT_TRUE(secureboot_source::parse_efivarfs_byte(efivar(1), value, error)) << error;
  EXPECT_EQ(1, value);
  ASSERT_TRUE(secureboot_source::parse_efivarfs_byte(efivar(0), value, error)) << error;
  EXPECT_EQ(0, value);
}

TEST(secureboot_linux, a_truncated_variable_is_an_error_not_a_zero) {
  long long value = -1;
  std::string error;
  EXPECT_FALSE(secureboot_source::parse_efivarfs_byte(std::string("\x07\x00\x00\x00", 4), value, error));
  EXPECT_NE(error.find("got 4 byte(s)"), std::string::npos) << error;
  EXPECT_FALSE(secureboot_source::parse_efivarfs_byte("", value, error));
}

TEST(secureboot_linux, no_efi_directory_is_a_legacy_bios_boot) {
  temp_dir t;
  std::string error;
  const auto rows = read_secureboot(t.path() + "/does-not-exist", error);
  EXPECT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(0, rows[0]->supported);
  EXPECT_EQ(0, rows[0]->enabled);
  EXPECT_FALSE(rows[0]->setup_mode);
}

TEST(secureboot_linux, enabled_and_in_user_mode) {
  temp_dir t;
  t.mkdir("efivars");
  t.write("efivars/SecureBoot-" + guid, efivar(1));
  t.write("efivars/SetupMode-" + guid, efivar(0));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(1, rows[0]->supported);
  EXPECT_EQ(1, rows[0]->enabled);
  EXPECT_EQ(0, rows[0]->setup_mode.value());
}

TEST(secureboot_linux, disabled_in_setup_mode) {
  temp_dir t;
  t.mkdir("efivars");
  t.write("efivars/SecureBoot-" + guid, efivar(0));
  t.write("efivars/SetupMode-" + guid, efivar(1));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(1, rows[0]->supported);
  EXPECT_EQ(0, rows[0]->enabled);
  EXPECT_EQ(1, rows[0]->setup_mode.value());
}

TEST(secureboot_linux, without_setup_mode_the_keyword_is_unknown) {
  temp_dir t;
  t.mkdir("efivars");
  t.write("efivars/SecureBoot-" + guid, efivar(1));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(1, rows[0]->enabled);
  EXPECT_FALSE(rows[0]->setup_mode);
}

TEST(secureboot_linux, the_legacy_sysfs_interface_holds_the_bare_value) {
  temp_dir t;
  t.mkdir("vars");
  t.mkdir("vars/SecureBoot-" + guid);
  t.write("vars/SecureBoot-" + guid + "/data", std::string(1, '\x01'));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(1, rows[0]->supported);
  EXPECT_EQ(1, rows[0]->enabled);
}

TEST(secureboot_linux, uefi_firmware_without_secure_boot_is_not_supported) {
  temp_dir t;
  t.mkdir("efivars");
  t.write("efivars/BootCurrent-" + guid, std::string("\x06\x00\x00\x00\x01\x00", 6));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(0, rows[0]->supported);
  EXPECT_EQ(0, rows[0]->enabled);
}

TEST(secureboot_linux, an_unmounted_efivarfs_is_an_error_not_a_bios_boot) {
  temp_dir t;
  t.mkdir("efivars");  // the mount point exists, nothing is mounted on it
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  EXPECT_TRUE(rows.empty());
  EXPECT_NE(error.find("efivarfs not mounted"), std::string::npos) << error;
}

TEST(secureboot_linux, a_malformed_variable_is_an_error) {
  temp_dir t;
  t.mkdir("efivars");
  t.write("efivars/SecureBoot-" + guid, std::string("\x07\x00", 2));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  EXPECT_TRUE(rows.empty());
  EXPECT_NE(error.find("Malformed EFI variable"), std::string::npos) << error;
}

TEST(secureboot_linux, an_unreadable_variable_is_an_error) {
  // A read error (here EISDIR, which holds even for root, where a 0000 mode
  // would not) must not read as "variable absent" and so as "not supported".
  temp_dir t;
  t.mkdir("efivars");
  t.mkdir("efivars/SecureBoot-" + guid);
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  EXPECT_TRUE(rows.empty());
  EXPECT_NE(error.find("Failed to read"), std::string::npos) << error;
}
