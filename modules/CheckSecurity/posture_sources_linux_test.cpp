// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Linux readers behind check_secureboot, check_local_accounts and
// check_group_members, against fixtures: efivars byte layouts in a temporary
// directory tree, and passwd/shadow/group content as strings. The command
// bodies themselves are covered by check_posture_commands_test.cpp.

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "account_files_linux.hpp"
#include "check_group_members.hpp"
#include "check_local_accounts.hpp"
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
  EXPECT_NE(error.find("got 0 byte(s)"), std::string::npos) << error;
  EXPECT_EQ(-1, value);  // a failed parse leaves the value alone
}

TEST(secureboot_linux, a_variable_longer_than_one_byte_is_not_a_boolean) {
  // The first data byte of some other variable is not a Secure Boot state.
  long long value = -1;
  std::string error;
  EXPECT_FALSE(secureboot_source::parse_efivarfs_byte(efivar(1) + '\x00', value, error));
  EXPECT_NE(error.find("got 6 byte(s)"), std::string::npos) << error;
  EXPECT_FALSE(secureboot_source::parse_efivarfs_byte(std::string(500, '\x01'), value, error));
  EXPECT_NE(error.find("more than"), std::string::npos) << error;
  EXPECT_EQ(-1, value);
}

TEST(secureboot_linux, the_legacy_data_file_is_exactly_the_value_byte) {
  long long value = -1;
  std::string error;
  ASSERT_TRUE(secureboot_source::parse_sysfs_byte(std::string(1, '\x01'), value, error)) << error;
  EXPECT_EQ(1, value);
  ASSERT_TRUE(secureboot_source::parse_sysfs_byte(std::string(1, '\x00'), value, error)) << error;
  EXPECT_EQ(0, value);
  ASSERT_TRUE(secureboot_source::parse_sysfs_byte(std::string(1, '\xff'), value, error)) << error;
  EXPECT_EQ(255, value);  // a byte, never sign-extended
  EXPECT_FALSE(secureboot_source::parse_sysfs_byte("", value, error));
  EXPECT_FALSE(secureboot_source::parse_sysfs_byte(efivar(1), value, error));  // efivarfs layout in the wrong place
  EXPECT_NE(error.find("got 5 byte(s)"), std::string::npos) << error;
}

TEST(secureboot_linux, a_flag_is_zero_or_one_and_nothing_else) {
  using secureboot_source::flag_state;
  EXPECT_EQ(flag_state::off, secureboot_source::decode_flag(0).state);
  EXPECT_EQ(flag_state::on, secureboot_source::decode_flag(1).state);
  for (const long long odd : {2LL, 255LL, -1LL}) {
    const auto f = secureboot_source::decode_flag(odd);
    EXPECT_EQ(flag_state::unreadable, f.state) << odd;
    EXPECT_NE(f.error.find("unexpected value " + std::to_string(odd)), std::string::npos) << f.error;
  }
}

// --- build_state(): the decision, without a filesystem -----------------------

namespace {

secureboot_source::efi_flag flag(const secureboot_source::flag_state state, const std::string &error = "") {
  secureboot_source::efi_flag f;
  f.state = state;
  f.error = error;
  return f;
}

}  // namespace

TEST(secureboot_state, secure_boot_on_and_off_are_supported) {
  using secureboot_source::flag_state;
  using secureboot_source::variable_listing;
  secureboot_filter::filter_obj row;
  std::string error;
  ASSERT_TRUE(secureboot_source::build_state(flag(flag_state::on), flag(flag_state::off), variable_listing::some, "/efi", row, error));
  EXPECT_EQ(1, row.supported);
  EXPECT_EQ(1, row.enabled);
  EXPECT_EQ(0, row.setup_mode.value());

  ASSERT_TRUE(secureboot_source::build_state(flag(flag_state::off), flag(flag_state::on), variable_listing::some, "/efi", row, error));
  EXPECT_EQ(1, row.supported);
  EXPECT_EQ(0, row.enabled);
  EXPECT_EQ(1, row.setup_mode.value());
  EXPECT_TRUE(error.empty()) << error;
}

TEST(secureboot_state, setup_mode_is_auxiliary) {
  using secureboot_source::flag_state;
  using secureboot_source::variable_listing;
  for (const flag_state sm : {flag_state::absent, flag_state::unreadable}) {
    secureboot_filter::filter_obj row;
    row.setup_mode = 1;  // must not survive from an earlier use of the row
    std::string error;
    ASSERT_TRUE(secureboot_source::build_state(flag(flag_state::on), flag(sm, "boom"), variable_listing::some, "/efi", row, error));
    EXPECT_EQ(1, row.enabled);
    EXPECT_FALSE(row.setup_mode);
    EXPECT_TRUE(error.empty()) << error;
  }
}

TEST(secureboot_state, an_unreadable_secure_boot_is_an_error_whatever_else_is_known) {
  using secureboot_source::flag_state;
  using secureboot_source::variable_listing;
  secureboot_filter::filter_obj row;
  std::string error;
  EXPECT_FALSE(secureboot_source::build_state(flag(flag_state::unreadable, "why"), flag(flag_state::off), variable_listing::some, "/efi", row, error));
  EXPECT_EQ("why", error);
  EXPECT_EQ(0, row.supported);
  EXPECT_EQ(0, row.enabled);
}

TEST(secureboot_state, an_absent_secure_boot_depends_on_whether_variables_are_visible) {
  using secureboot_source::flag_state;
  using secureboot_source::variable_listing;
  secureboot_filter::filter_obj row;
  std::string error;
  // Other variables are listed: the firmware simply has no Secure Boot.
  ASSERT_TRUE(secureboot_source::build_state(flag(flag_state::absent), flag(flag_state::absent), variable_listing::some, "/efi", row, error));
  EXPECT_EQ(0, row.supported);
  EXPECT_EQ(0, row.enabled);
  EXPECT_FALSE(row.setup_mode);
  // Nothing is listed: efivarfs is not mounted, and "not supported" would be a guess.
  EXPECT_FALSE(secureboot_source::build_state(flag(flag_state::absent), flag(flag_state::absent), variable_listing::none, "/efi", row, error));
  EXPECT_EQ("Cannot read UEFI variables: /efi/efivars is empty (efivarfs not mounted?)", error);
  // The listing itself failed (say, permission denied).
  error.clear();
  EXPECT_FALSE(secureboot_source::build_state(flag(flag_state::absent), flag(flag_state::absent), variable_listing::failed, "/efi", row, error));
  EXPECT_NE(error.find("Cannot read UEFI variables"), std::string::npos) << error;
}

// --- read_flag() and gather_from(), against a fixture tree --------------------

TEST(secureboot_linux, read_flag_tells_absent_from_unreadable) {
  using secureboot_source::flag_state;
  temp_dir t;
  t.mkdir("efivars");
  t.write("efivars/SecureBoot-" + guid, efivar(1));
  t.write("efivars/SetupMode-" + guid, efivar(0));
  t.write("efivars/Truncated-" + guid, std::string("\x07\x00\x00\x00", 4));
  t.write("efivars/Empty-" + guid, "");  // efivarfs shows a variable being written as a 0-byte file
  t.write("efivars/Wide-" + guid, efivar(1) + '\x00');
  t.write("efivars/Odd-" + guid, efivar(2));
  EXPECT_EQ(flag_state::on, secureboot_source::read_flag(t.path(), "SecureBoot").state);
  EXPECT_EQ(flag_state::off, secureboot_source::read_flag(t.path(), "SetupMode").state);
  EXPECT_EQ(flag_state::absent, secureboot_source::read_flag(t.path(), "NoSuchVariable").state);
  EXPECT_TRUE(secureboot_source::read_flag(t.path(), "NoSuchVariable").error.empty());
  for (const char *name : {"Truncated", "Empty", "Wide", "Odd"}) {
    const auto f = secureboot_source::read_flag(t.path(), name);
    EXPECT_EQ(flag_state::unreadable, f.state) << name;
    // The message names the file, so the operator can look at it.
    EXPECT_NE(f.error.find("Malformed EFI variable " + t.path() + "/efivars/" + name + "-" + guid), std::string::npos) << f.error;
  }
  EXPECT_NE(secureboot_source::read_flag(t.path(), "Odd").error.find("unexpected value 2"), std::string::npos);
}

TEST(secureboot_linux, efivarfs_wins_over_the_legacy_interface) {
  using secureboot_source::flag_state;
  temp_dir t;
  t.mkdir("efivars");
  t.mkdir("vars");
  t.mkdir("vars/SecureBoot-" + guid);
  t.write("efivars/SecureBoot-" + guid, efivar(1));
  t.write("vars/SecureBoot-" + guid + "/data", std::string(1, '\x00'));
  EXPECT_EQ(flag_state::on, secureboot_source::read_flag(t.path(), "SecureBoot").state);

  // A malformed efivarfs file is an error; it does not fall through to the
  // legacy file and take a second opinion.
  t.write("efivars/SecureBoot-" + guid, "");
  EXPECT_EQ(flag_state::unreadable, secureboot_source::read_flag(t.path(), "SecureBoot").state);
}

TEST(secureboot_linux, the_legacy_interface_is_validated_too) {
  using secureboot_source::flag_state;
  temp_dir t;
  t.mkdir("vars");
  for (const char *name : {"Empty", "Wide", "Odd", "Good"}) t.mkdir(std::string("vars/") + name + "-" + guid);
  t.write("vars/Empty-" + guid + "/data", "");
  t.write("vars/Wide-" + guid + "/data", std::string(2, '\x01'));
  t.write("vars/Odd-" + guid + "/data", std::string(1, '\x07'));
  t.write("vars/Good-" + guid + "/data", std::string(1, '\x00'));
  EXPECT_EQ(flag_state::unreadable, secureboot_source::read_flag(t.path(), "Empty").state);
  EXPECT_EQ(flag_state::unreadable, secureboot_source::read_flag(t.path(), "Wide").state);
  EXPECT_EQ(flag_state::unreadable, secureboot_source::read_flag(t.path(), "Odd").state);
  EXPECT_EQ(flag_state::off, secureboot_source::read_flag(t.path(), "Good").state);
  // A variable directory without its data file is absent, not an error.
  t.mkdir("vars/NoData-" + guid);
  EXPECT_EQ(flag_state::absent, secureboot_source::read_flag(t.path(), "NoData").state);
}

TEST(secureboot_linux, an_efi_path_that_is_not_a_directory_is_a_bios_boot) {
  temp_dir t;
  t.write("efi", "not a directory");
  std::string error;
  const auto rows = read_secureboot(t.path() + "/efi", error);
  EXPECT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(0, rows[0]->supported);
}

TEST(secureboot_linux, an_unexpected_value_is_an_error_not_disabled) {
  temp_dir t;
  t.mkdir("efivars");
  t.write("efivars/SecureBoot-" + guid, efivar(2));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  EXPECT_TRUE(rows.empty());
  EXPECT_NE(error.find("unexpected value 2 (expected 0 or 1)"), std::string::npos) << error;
}

TEST(secureboot_linux, an_oversized_variable_is_an_error) {
  temp_dir t;
  t.mkdir("efivars");
  t.write("efivars/SecureBoot-" + guid, std::string(100000, '\x01'));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  EXPECT_TRUE(rows.empty());
  EXPECT_NE(error.find("Malformed EFI variable"), std::string::npos) << error;
  EXPECT_NE(error.find("more than"), std::string::npos) << error;
}

TEST(secureboot_linux, a_malformed_setup_mode_leaves_only_that_keyword_unknown) {
  for (const std::string &setup_mode : {std::string(""), efivar(2), efivar(1) + "x"}) {
    temp_dir t;
    t.mkdir("efivars");
    t.write("efivars/SecureBoot-" + guid, efivar(1));
    t.write("efivars/SetupMode-" + guid, setup_mode);
    std::string error;
    const auto rows = read_secureboot(t.path(), error);
    ASSERT_TRUE(error.empty()) << error;
    ASSERT_EQ(1u, rows.size());
    EXPECT_EQ(1, rows[0]->enabled);
    EXPECT_FALSE(rows[0]->setup_mode);
  }
}

TEST(secureboot_linux, setup_mode_is_read_from_the_legacy_interface_too) {
  temp_dir t;
  t.mkdir("vars");
  t.mkdir("vars/SecureBoot-" + guid);
  t.mkdir("vars/SetupMode-" + guid);
  t.write("vars/SecureBoot-" + guid + "/data", std::string(1, '\x00'));
  t.write("vars/SetupMode-" + guid + "/data", std::string(1, '\x01'));
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(1, rows[0]->supported);
  EXPECT_EQ(0, rows[0]->enabled);
  EXPECT_EQ(1, rows[0]->setup_mode.value());
}

TEST(secureboot_linux, legacy_firmware_without_secure_boot_is_not_supported) {
  temp_dir t;
  t.mkdir("vars");
  t.mkdir("vars/BootCurrent-" + guid);
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(0, rows[0]->supported);
}

TEST(secureboot_linux, an_efi_directory_with_no_variable_interface_is_an_error) {
  temp_dir t;  // the directory exists (a UEFI boot) but neither efivars nor vars does
  std::string error;
  const auto rows = read_secureboot(t.path(), error);
  EXPECT_TRUE(rows.empty());
  EXPECT_NE(error.find("Cannot read UEFI variables"), std::string::npos) << error;
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

// ---------------------------------------------------------------------------
// check_local_accounts
// ---------------------------------------------------------------------------

namespace {

// A trimmed stock Debian host, plus the findings the check exists for.
const std::string passwd_fixture =
    "root:x:0:0:root:/root:/bin/bash\n"
    "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin\n"
    "sync:x:4:65534:sync:/bin:/bin/sync\n"
    "systemd-network:x:998:998:systemd Network Management:/:/usr/sbin/nologin\n"
    "sshd:x:100:65534::/run/sshd:/usr/sbin/nologin\n"
    "# a comment\n"
    "\n"
    "alice:x:1000:1000:Alice,,,:/home/alice:/bin/bash\n"
    "bob:x:1001:1001::/home/bob:/bin/zsh\n"
    "kiosk:x:1002:1002::/home/kiosk:/bin/bash\n"
    "expired:x:1003:1003::/home/expired:/bin/bash\n"
    "toor:x:0:0:backdoor:/root:/bin/sh\n"
    "legacy::1004:1004::/home/legacy:\n"
    "noshadow:x:1005:1005::/home/noshadow:/bin/bash\n"
    "+@netgroup::::::\n"
    "broken:x:notanumber:1:::\n";

// Hashes are fake but shaped like the real thing.
const std::string shadow_fixture =
    "root:$y$j9T$abcdefgh$ABCDEFGHIJKLMNOPQRSTUVWXYZ012345678901:19700:0:99999:7:::\n"
    "daemon:*:19700:0:99999:7:::\n"
    "sync:*:19700:0:99999:7:::\n"
    "systemd-network:!*:19700::::::\n"
    "sshd:!:19700::::::\n"
    "alice:$6$salt$hashhashhash:19000:0:90:7:::\n"
    "bob:!$6$salt$hashhashhash:19700:0:99999:7:::\n"
    "kiosk::19700:0:99999:7:::\n"
    "expired:$6$salt$hashhashhash:19700:0:99999:7::19000:\n"
    "toor:$6$salt$hashhashhash:0:0:99999:7:::\n";

const long long now = 20000LL * 86400 + 3600;  // day 20000, 01:00 UTC

std::vector<local_accounts_filter::filter_obj_ptr> build(const bool shadow_readable) {
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd(passwd_fixture, skipped);
  const auto shadow = local_accounts_source::parse_shadow(shadow_fixture, skipped);
  std::vector<local_accounts_filter::filter_obj_ptr> out;
  local_accounts_source::build_accounts(passwd, shadow_readable ? &shadow : nullptr, shadow_readable ? "" : "Cannot read /etc/shadow: Permission denied", now,
                                        out);
  return out;
}

local_accounts_filter::filter_obj_ptr find(const std::vector<local_accounts_filter::filter_obj_ptr> &rows, const std::string &name) {
  for (const auto &r : rows) {
    if (r->name == name) return r;
  }
  return nullptr;
}

}  // namespace

TEST(local_accounts_linux, password_fields_are_classified_not_copied) {
  EXPECT_EQ("empty", local_accounts_source::classify_password(""));
  EXPECT_EQ("locked", local_accounts_source::classify_password("!"));
  EXPECT_EQ("locked", local_accounts_source::classify_password("!!"));
  EXPECT_EQ("locked", local_accounts_source::classify_password("!*"));
  EXPECT_EQ("locked", local_accounts_source::classify_password("!$6$salt$hash"));
  EXPECT_EQ("none", local_accounts_source::classify_password("*"));
  EXPECT_EQ("none", local_accounts_source::classify_password("x"));
  EXPECT_EQ("set", local_accounts_source::classify_password("$6$salt$hash"));
  EXPECT_EQ("set", local_accounts_source::classify_password("$y$j9T$salt$hash"));
  EXPECT_EQ("set", local_accounts_source::classify_password("abJnggxhB/yWI"));  // traditional DES
}

TEST(local_accounts_linux, nologin_style_shells_are_not_interactive) {
  EXPECT_FALSE(local_accounts_source::is_interactive_shell("/usr/sbin/nologin"));
  EXPECT_FALSE(local_accounts_source::is_interactive_shell("/sbin/nologin"));
  EXPECT_FALSE(local_accounts_source::is_interactive_shell("/bin/false"));
  EXPECT_FALSE(local_accounts_source::is_interactive_shell("/usr/bin/false"));
  EXPECT_FALSE(local_accounts_source::is_interactive_shell("/bin/sync"));
  EXPECT_FALSE(local_accounts_source::is_interactive_shell("/sbin/shutdown"));
  EXPECT_FALSE(local_accounts_source::is_interactive_shell("/sbin/halt"));
  EXPECT_TRUE(local_accounts_source::is_interactive_shell("/bin/bash"));
  EXPECT_TRUE(local_accounts_source::is_interactive_shell("/usr/bin/zsh"));
  EXPECT_TRUE(local_accounts_source::is_interactive_shell(""));  // login(1) uses /bin/sh
}

TEST(local_accounts_linux, malformed_and_compat_lines_are_skipped) {
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd(passwd_fixture, skipped);
  // "broken", by its line number; the comment, blank and +@netgroup lines are not entries
  EXPECT_EQ((std::vector<std::size_t>{16}), skipped);
  ASSERT_EQ(12u, passwd.size());
  EXPECT_EQ("root", passwd[0].name);
  EXPECT_EQ(0, passwd[0].uid);
  EXPECT_EQ("/bin/bash", passwd[0].shell);

  const auto shadow = local_accounts_source::parse_shadow("root:*:1:2:3:4:5:6:\nshort:*:1\nwindows:*:1:0:99999:7:::\r\n", skipped);
  // The malformed line stays, flagged, so its account is not mistaken for one
  // without an entry.
  EXPECT_EQ((std::vector<std::size_t>{2}), skipped);
  ASSERT_EQ(3u, shadow.size());
  EXPECT_TRUE(shadow[1].malformed);
  EXPECT_EQ("short", shadow[1].name);
  EXPECT_EQ(2u, shadow[1].line);
  EXPECT_FALSE(shadow[2].malformed);
  EXPECT_EQ("windows", shadow[2].name);  // a CRLF file still parses
}

// --- the line handling shared by the passwd, shadow and group readers --------

TEST(account_files, fields_are_split_on_every_separator) {
  EXPECT_EQ((std::vector<std::string>{"a", "", "c"}), account_files::split_fields("a::c"));
  EXPECT_EQ((std::vector<std::string>{""}), account_files::split_fields(""));
  EXPECT_EQ((std::vector<std::string>{"a", ""}), account_files::split_fields("a:"));
  EXPECT_EQ((std::vector<std::string>{"x", "y"}), account_files::split_fields("x,y", ','));
}

TEST(account_files, numbers_are_digits_only_and_empty_is_minus_one) {
  long long v = 7;
  EXPECT_TRUE(account_files::parse_number("", v));
  EXPECT_EQ(-1, v);
  EXPECT_TRUE(account_files::parse_number("0", v));
  EXPECT_EQ(0, v);
  EXPECT_TRUE(account_files::parse_number("9223372036854775807", v));
  EXPECT_EQ(9223372036854775807LL, v);
  for (const char *bad : {"-1", "+0", " 0", "0 ", "0x0", "1a", "9223372036854775808", "99999999999999999999999"}) {
    v = 7;
    EXPECT_FALSE(account_files::parse_number(bad, v)) << bad;
    EXPECT_EQ(7, v) << bad;
  }
}

TEST(account_files, entries_are_numbered_as_an_editor_shows_them) {
  std::vector<std::pair<std::string, std::size_t>> seen;
  account_files::for_each_entry("# c\n\n  \t\na:1\r\n+nis\n-nis\n  b:2\n\t# indented comment\nlast",
                                [&](const std::string &line, const std::size_t n) { seen.emplace_back(line, n); });
  // Leading blanks are dropped, as glibc does before it looks at the line.
  EXPECT_EQ((std::vector<std::pair<std::string, std::size_t>>{{"a:1", 4}, {"b:2", 7}, {"last", 9}}), seen);
}

TEST(account_files, unparsed_lines_are_named_by_number_only) {
  EXPECT_EQ("", account_files::unparsed_lines_error("/etc/passwd", {}));
  EXPECT_EQ("1 unparseable line(s) in /etc/passwd (line 3)", account_files::unparsed_lines_error("/etc/passwd", {3}));
  EXPECT_EQ("7 unparseable line(s) in f (line 1, 2, 3, 4, 5, ...)", account_files::unparsed_lines_error("f", {1, 2, 3, 4, 5, 6, 7}));
}

// --- what glibc accepts, checked against fgetpwent()/fgetspent() -------------

TEST(local_accounts_linux, a_short_passwd_line_is_still_an_account) {
  // glibc needs four fields: "toor::0:0" is a passwordless root.
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd("toor::0:0\nfive:x:5:5:gecos\n  lead:x:8:8::/:/bin/sh\n", skipped);
  EXPECT_TRUE(skipped.empty());
  ASSERT_EQ(3u, passwd.size());
  EXPECT_EQ("toor", passwd[0].name);
  EXPECT_EQ("", passwd[0].password);
  EXPECT_EQ(0, passwd[0].uid);
  EXPECT_EQ("", passwd[0].home);
  EXPECT_EQ("", passwd[0].shell);
  EXPECT_EQ("lead", passwd[2].name);

  std::vector<local_accounts_filter::filter_obj_ptr> out;
  local_accounts_source::build_accounts(passwd, nullptr, "Cannot read /etc/shadow: Permission denied", now, out);
  EXPECT_EQ("empty", out[0]->password_state);
  EXPECT_EQ(0, out[0]->get_password_required().value());
  EXPECT_EQ(1, out[0]->get_enabled().value());
  EXPECT_EQ(1, out[0]->get_is_extra_uid0());
  EXPECT_EQ(1, out[0]->get_interactive().value());
}

TEST(local_accounts_linux, the_shell_is_the_rest_of_the_line) {
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd("long:x:7:7:g:/h:/bin/sh:extra\n", skipped);
  EXPECT_TRUE(skipped.empty());
  ASSERT_EQ(1u, passwd.size());
  EXPECT_EQ("/h", passwd[0].home);
  EXPECT_EQ("/bin/sh:extra", passwd[0].shell);
}

TEST(local_accounts_linux, passwd_lines_that_cannot_be_parsed_are_reported_by_line) {
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd(
      "root:x:0:0::/root:/bin/sh\n"
      "three:x:5\n"
      "only2:x\n"
      "nouid:x::9::/:/bin/sh\n"
      "nogid:x:9:::/:/bin/sh\n"
      "alpha:x:1a:1::/:/bin/sh\n"
      ":x:1:1::/:/bin/sh\n"
      "signed:x:+0:0::/:/bin/sh\n",
      skipped);
  ASSERT_EQ(1u, passwd.size());
  EXPECT_EQ((std::vector<std::size_t>{2, 3, 4, 5, 6, 7, 8}), skipped);
}

TEST(local_accounts_linux, an_unparseable_passwd_line_fails_the_check) {
  // "signed" is uid 0 to glibc (strtoul takes the sign): answering for the
  // other accounts would hide a second root.
  temp_dir t;
  t.write("passwd", "root:x:0:0::/root:/bin/sh\nsigned:x:+0:0::/:/bin/sh\n");
  t.write("shadow", "root:*:19700:0:99999:7:::\n", 0600);
  std::vector<local_accounts_filter::filter_obj_ptr> out;
  std::string error;
  local_accounts_source::gather_from(t.path() + "/passwd", t.path() + "/shadow", now, out, error);
  EXPECT_TRUE(out.empty());
  EXPECT_NE(error.find("1 unparseable line(s) in " + t.path() + "/passwd (line 2)"), std::string::npos) << error;
  EXPECT_EQ(error.find("signed"), std::string::npos) << error;  // a line number, never the line
}

TEST(local_accounts_linux, the_shadow_forms_glibc_returns_are_accepted) {
  std::vector<std::size_t> skipped;
  const auto shadow = local_accounts_source::parse_shadow(
      "nine:*:19000:0:99999:7:30:20000:\n"
      "eight:*:19000:0:99999:7:30:20000\n"
      "old5:*:19000:0:99999\n",
      skipped);
  EXPECT_TRUE(skipped.empty());
  ASSERT_EQ(3u, shadow.size());
  for (const auto &s : shadow) EXPECT_FALSE(s.malformed) << s.name;
  EXPECT_EQ(30, shadow[0].inactive);
  EXPECT_EQ(20000, shadow[1].expire);
  EXPECT_EQ(-1, shadow[2].inactive);
  EXPECT_EQ(-1, shadow[2].expire);
  EXPECT_EQ(99999, shadow[2].max_days);
}

TEST(local_accounts_linux, shadow_lines_glibc_rejects_are_malformed_and_carry_nothing) {
  std::vector<std::size_t> skipped;
  const auto shadow = local_accounts_source::parse_shadow(
      "two:$6$a$b\n"
      "six:$6$a$b:19000:0:99999:7\n"
      "minus:$6$a$b:19000:0:99999:7:-1:-1:\n"
      "alpha:$6$a$b:abc:0:99999:7:::\n"
      "warn:$6$a$b:19000:0:99999:x:::\n"
      "ten:$6$a$b:19000:0:99999:7::::\n"
      "name1\n"
      ":$6$a$b:19000:0:99999:7:::\n",
      skipped);
  EXPECT_EQ((std::vector<std::size_t>{1, 2, 3, 4, 5, 6, 7, 8}), skipped);
  ASSERT_EQ(7u, shadow.size());  // the nameless line names no account
  for (const auto &s : shadow) {
    EXPECT_TRUE(s.malformed) << s.name;
    EXPECT_EQ("unknown", s.state) << s.name;
    EXPECT_EQ(-1, s.last_change) << s.name;
    EXPECT_EQ(-1, s.max_days) << s.name;
  }
}

TEST(local_accounts_linux, a_shadow_day_count_beyond_an_int_is_malformed) {
  // glibc stores the day counts through an int, and password_changed is the
  // count times 86400: a larger value is refused rather than overflowed.
  std::vector<std::size_t> skipped;
  const auto shadow = local_accounts_source::parse_shadow(
      "ok:$6$a$b:2147483647:0:2147483647:7:2147483647:2147483647:\n"
      "lastchg:$6$a$b:2147483648:0:99999:7:::\n"
      "huge:$6$a$b:9223372036854775807:0:99999:7:::\n"
      "max:$6$a$b:19000:0:2147483648:7:::\n"
      "expire:$6$a$b:19000:0:99999:7::2147483648:\n",
      skipped);
  EXPECT_EQ((std::vector<std::size_t>{2, 3, 4, 5}), skipped);
  ASSERT_EQ(5u, shadow.size());
  EXPECT_FALSE(shadow[0].malformed);
  EXPECT_EQ(2147483647LL, shadow[0].last_change);
  for (std::size_t i = 1; i < shadow.size(); ++i) EXPECT_TRUE(shadow[i].malformed) << shadow[i].name;
}

TEST(local_accounts_linux, a_malformed_shadow_entry_is_unknown_and_says_which_line) {
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd("bad:x:2000:2000::/home/bad:/bin/bash\ngood:x:2001:2001::/home/good:/bin/bash\n", skipped);
  const auto shadow = local_accounts_source::parse_shadow("good::19000:0:99999:7:::\nbad::19000:0:99999:7:-1:-1:\n", skipped);
  std::vector<local_accounts_filter::filter_obj_ptr> out;
  local_accounts_source::build_accounts(passwd, &shadow, "", now, out);
  ASSERT_EQ(2u, out.size());
  EXPECT_EQ("unknown", out[0]->password_state);
  EXPECT_FALSE(out[0]->get_password_required());
  EXPECT_FALSE(out[0]->get_enabled());
  EXPECT_EQ("Unparseable entry for account 'bad' in /etc/shadow (line 2)", out[0]->shadow_error);
  EXPECT_FALSE(out[0]->shadow_hint.empty());
  // The account next to it is unaffected.
  EXPECT_EQ("empty", out[1]->password_state);
  EXPECT_TRUE(out[1]->shadow_error.empty());
}

TEST(local_accounts_linux, the_first_parseable_shadow_entry_wins) {
  // getspnam() skips the line it cannot parse and returns the next one.
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd("dup:x:2000:2000::/home/dup:/bin/bash\n", skipped);
  for (const char *content : {"dup:broken\ndup::19000:0:99999:7:::\ndup:*:19000:0:99999:7:::\n", "dup::19000:0:99999:7:::\ndup:broken\n"}) {
    const auto shadow = local_accounts_source::parse_shadow(content, skipped);
    std::vector<local_accounts_filter::filter_obj_ptr> out;
    local_accounts_source::build_accounts(passwd, &shadow, "", now, out);
    EXPECT_EQ("empty", out[0]->password_state) << content;
    EXPECT_TRUE(out[0]->shadow_error.empty()) << content;
  }
}

TEST(local_accounts_linux, every_spelling_pam_treats_as_shadowed_reads_shadow) {
  EXPECT_TRUE(local_accounts_source::defers_to_shadow("alice", "x"));
  EXPECT_TRUE(local_accounts_source::defers_to_shadow("alice", "##alice"));
  EXPECT_TRUE(local_accounts_source::defers_to_shadow("alice", "*NP*"));
  EXPECT_FALSE(local_accounts_source::defers_to_shadow("alice", "##bob"));
  EXPECT_FALSE(local_accounts_source::defers_to_shadow("alice", "##"));
  EXPECT_FALSE(local_accounts_source::defers_to_shadow("alice", "*"));
  EXPECT_FALSE(local_accounts_source::defers_to_shadow("alice", ""));
  EXPECT_FALSE(local_accounts_source::defers_to_shadow("alice", "X"));

  std::vector<std::size_t> skipped;
  const auto passwd =
      local_accounts_source::parse_passwd("hash:##hash:2000:2000::/h:/bin/sh\nnis:*NP*:2001:2001::/h:/bin/sh\nother:##hash:2002:2002::/h:/bin/sh\n", skipped);
  const auto shadow = local_accounts_source::parse_shadow("hash::19000:0:99999:7:::\nnis:$6$s$h:19000:0:90:7:::\n", skipped);
  std::vector<local_accounts_filter::filter_obj_ptr> out;
  local_accounts_source::build_accounts(passwd, &shadow, "", now, out);
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ("empty", out[0]->password_state);  // from shadow, not "none" from the marker
  EXPECT_EQ(0, out[0]->get_password_required().value());
  EXPECT_EQ("set", out[1]->password_state);
  EXPECT_EQ(1, out[1]->get_password_expires().value());
  // "##hash" on another account is not a shadow marker: a field nothing matches.
  EXPECT_EQ("none", out[2]->password_state);
  EXPECT_TRUE(out[2]->shadow_error.empty());

  // ...and without shadow access they are unknown, not guessed.
  out.clear();
  local_accounts_source::build_accounts(passwd, nullptr, "Cannot read /etc/shadow: Permission denied", now, out);
  EXPECT_EQ("unknown", out[0]->password_state);
  EXPECT_FALSE(out[0]->shadow_error.empty());
  EXPECT_FALSE(out[1]->shadow_error.empty());
  EXPECT_TRUE(out[2]->shadow_error.empty());
}

TEST(local_accounts_linux, the_older_des_hash_forms_are_passwords_too) {
  EXPECT_EQ("set", local_accounts_source::classify_password("abJnggxhB/yWIabcdefghijk"));  // bigcrypt: DES past 13 characters
  EXPECT_EQ("set", local_accounts_source::classify_password("_J9..rasmBYk8r9AiWNc"));      // BSDi extended DES
  EXPECT_EQ("none", local_accounts_source::classify_password("_J9..rasm"));                // too short for BSDi
  EXPECT_EQ("none", local_accounts_source::classify_password("_J9..rasmBYk8r9AiW*c"));     // not the crypt alphabet
  EXPECT_EQ("none", local_accounts_source::classify_password("abJnggxhB/yW"));             // 12 characters: no hash is that short
  EXPECT_EQ("none", local_accounts_source::classify_password("*LK*"));
  EXPECT_EQ("none", local_accounts_source::classify_password("NP"));
  EXPECT_EQ("none", local_accounts_source::classify_password("abJnggxhB/yW*abcdefgh"));  // a '*' anywhere: nothing hashes to it
}

namespace {

// One shadowed account "u" with the given shadow line, as build_accounts sees it.
local_accounts_filter::filter_obj_ptr account_with_shadow(const std::string &shadow_line) {
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd("u:x:2000:2000::/home/u:/bin/bash\n", skipped);
  const auto shadow = local_accounts_source::parse_shadow(shadow_line + "\n", skipped);
  std::vector<local_accounts_filter::filter_obj_ptr> out;
  local_accounts_source::build_accounts(passwd, &shadow, "", now, out);
  return out.at(0);
}

}  // namespace

TEST(local_accounts_linux, a_password_expired_past_its_inactivity_period_disables_the_account) {
  // Today is day 20000. Changed on day 19000 with a 90 day maximum: expired on
  // day 19090. PAM refuses the account once max + inactive days have passed.
  EXPECT_EQ(1, account_with_shadow("u:$6$s$h:19000:0:90:7:30::")->get_disabled().value());
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h:19000:0:90:7:30::")->get_enabled().value());
  EXPECT_EQ(1, account_with_shadow("u:$6$s$h:19000:0:90:7:0::")->get_disabled().value());
  // The boundary: 1000 days have passed.
  EXPECT_EQ(1, account_with_shadow("u:$6$s$h:19000:0:900:7:100::")->get_disabled().value());
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h:19000:0:900:7:101::")->get_disabled().value());
}

TEST(local_accounts_linux, inactivity_needs_every_field_it_is_computed_from) {
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h:19000:0:90:7:::")->get_disabled().value());       // no inactivity period
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h:19000:0::7:30::")->get_disabled().value());       // password never expires
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h::0:90:7:30::")->get_disabled().value());          // no change recorded
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h:0:0:90:7:30::")->get_disabled().value());         // change forced at next logon
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h:20001:0:90:7:30::")->get_disabled().value());     // changed "tomorrow": clock skew
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h:19000:0:99999:7:30::")->get_disabled().value());  // 99999 + 30 days is centuries away
  // The largest day count a line may hold is "never", not a wrapped date.
  // (Anything larger makes the line unparseable: see
  // a_shadow_day_count_beyond_an_int_is_malformed.)
  EXPECT_EQ(0, account_with_shadow("u:$6$s$h:19000:0:2147483647:7:2147483647::")->get_disabled().value());
  // The expiry date still disables on its own.
  EXPECT_EQ(1, account_with_shadow("u:$6$s$h:19000:0:99999:7::19999:")->get_disabled().value());
}

TEST(local_accounts_linux, no_hash_survives_parsing) {
  std::vector<std::size_t> skipped;
  const auto shadow = local_accounts_source::parse_shadow(shadow_fixture, skipped);
  for (const auto &s : shadow) {
    EXPECT_EQ(s.state.find('$'), std::string::npos) << s.name;
  }
  for (const auto &r : build(true)) {
    EXPECT_EQ(r->password_state.find('$'), std::string::npos) << r->name;
    EXPECT_EQ(r->sid.find('$'), std::string::npos) << r->name;
  }
}

TEST(local_accounts_linux, system_accounts_are_never_passwordless) {
  // The default critical is `enabled = 1 and password_required = 0`: the
  // dozens of '*' / '!' system accounts must not trip it.
  const auto rows = build(true);
  for (const char *name : {"daemon", "sync", "systemd-network", "sshd"}) {
    const auto r = find(rows, name);
    ASSERT_TRUE(r) << name;
    EXPECT_EQ(1, r->get_enabled().value()) << name;
    EXPECT_EQ(1, r->get_password_required().value()) << name;
    EXPECT_EQ(0, r->get_interactive().value()) << name;
  }
  EXPECT_EQ("none", find(rows, "daemon")->password_state);
  EXPECT_EQ(0, find(rows, "daemon")->get_locked().value());
  EXPECT_EQ("locked", find(rows, "sshd")->password_state);
  EXPECT_EQ(1, find(rows, "sshd")->get_locked().value());
}

TEST(local_accounts_linux, root_maps_onto_the_builtin_administrator) {
  const auto root = find(build(true), "root");
  ASSERT_TRUE(root);
  EXPECT_EQ("0", root->sid);
  EXPECT_EQ(0, root->get_uid().value());
  EXPECT_EQ(0, root->get_gid().value());
  EXPECT_EQ("/root", root->home);
  EXPECT_EQ("/bin/bash", root->shell);
  EXPECT_EQ(1, root->get_is_builtin_admin());
  EXPECT_EQ(0, root->get_is_builtin_guest());
  EXPECT_EQ(0, root->get_is_extra_uid0());
  EXPECT_EQ("set", root->password_state);
  EXPECT_EQ(1, root->get_password_required().value());
  EXPECT_EQ(0, root->get_password_expires().value());  // 99999 = never
}

TEST(local_accounts_linux, a_second_uid0_account_is_flagged) {
  const auto toor = find(build(true), "toor");
  ASSERT_TRUE(toor);
  EXPECT_EQ(0, toor->get_is_builtin_admin());
  EXPECT_EQ(1, toor->get_is_extra_uid0());
  // lastchg 0 forces a change at next logon: not a 1970 change date.
  EXPECT_FALSE(toor->get_password_changed());
  EXPECT_FALSE(toor->get_password_age());
}

TEST(local_accounts_linux, an_empty_password_is_the_critical_finding) {
  const auto rows = build(true);
  const auto kiosk = find(rows, "kiosk");
  ASSERT_TRUE(kiosk);
  EXPECT_EQ("empty", kiosk->password_state);
  EXPECT_EQ(0, kiosk->get_password_required().value());
  EXPECT_EQ(1, kiosk->get_enabled().value());

  // An empty field in passwd itself (no shadow indirection) counts too, and
  // needs no shadow access to see.
  for (const bool readable : {true, false}) {
    const auto legacy = find(build(readable), "legacy");
    ASSERT_TRUE(legacy);
    EXPECT_EQ("empty", legacy->password_state);
    EXPECT_EQ(0, legacy->get_password_required().value());
    EXPECT_EQ(1, legacy->get_enabled().value());
    EXPECT_EQ(1, legacy->get_interactive().value());  // empty shell = /bin/sh
    EXPECT_TRUE(legacy->shadow_error.empty());
  }
}

TEST(local_accounts_linux, a_locked_password_is_locked_but_still_enabled) {
  // usermod -L stops password logons only; an SSH key still works, so the
  // account is not "disabled".
  const auto bob = find(build(true), "bob");
  ASSERT_TRUE(bob);
  EXPECT_EQ(1, bob->get_locked().value());
  EXPECT_EQ(1, bob->get_enabled().value());
  EXPECT_EQ("locked", bob->password_state);
}

TEST(local_accounts_linux, an_expired_account_is_disabled) {
  const auto expired = find(build(true), "expired");
  ASSERT_TRUE(expired);
  EXPECT_EQ(1, expired->get_disabled().value());
  EXPECT_EQ(0, expired->get_enabled().value());

  // Expiry day == today is already expired (pam_unix: today >= expire).
  std::vector<std::size_t> skipped;
  const auto passwd = local_accounts_source::parse_passwd("edge:x:2000:2000::/home/edge:/bin/bash\n", skipped);
  const auto shadow = local_accounts_source::parse_shadow("edge:$6$s$h:19000:0:99999:7::20000:\n", skipped);
  std::vector<local_accounts_filter::filter_obj_ptr> out;
  local_accounts_source::build_accounts(passwd, &shadow, "", now, out);
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ(1, out[0]->get_disabled().value());
}

TEST(local_accounts_linux, password_ageing_comes_from_shadow) {
  const auto alice = find(build(true), "alice");
  ASSERT_TRUE(alice);
  EXPECT_EQ(1, alice->get_password_expires().value());  // max 90 days
  EXPECT_EQ(19000LL * 86400, alice->get_password_changed().value());
  EXPECT_EQ(now - 19000LL * 86400, alice->get_password_age().value());
}

TEST(local_accounts_linux, an_unreadable_shadow_leaves_the_flags_unknown) {
  const auto rows = build(false);
  const auto root = find(rows, "root");
  ASSERT_TRUE(root);
  EXPECT_EQ("unknown", root->password_state);
  EXPECT_FALSE(root->get_enabled());
  EXPECT_FALSE(root->get_disabled());
  EXPECT_FALSE(root->get_locked());
  EXPECT_FALSE(root->get_password_required());
  EXPECT_FALSE(root->get_password_expires());
  EXPECT_FALSE(root->get_password_changed());
  EXPECT_EQ("Cannot read /etc/shadow: Permission denied", root->shadow_error);
  // passwd-derived keywords are unaffected.
  EXPECT_EQ(1, root->get_is_builtin_admin());
  EXPECT_EQ(1, find(rows, "toor")->get_is_extra_uid0());
  EXPECT_EQ(0, find(rows, "daemon")->get_interactive().value());
}

TEST(local_accounts_linux, an_account_missing_from_shadow_is_unknown_and_says_so) {
  // Its password state was never read: passing the default thresholds on it
  // would be the false negative the unreadable-shadow refusal exists to prevent.
  const auto r = find(build(true), "noshadow");
  ASSERT_TRUE(r);
  EXPECT_EQ("unknown", r->password_state);
  EXPECT_FALSE(r->get_password_required());
  EXPECT_FALSE(r->get_enabled());
  EXPECT_FALSE(r->get_locked());
  EXPECT_EQ("No entry for account 'noshadow' in /etc/shadow", r->shadow_error);
  EXPECT_NE(r->shadow_hint.find("pwck"), std::string::npos);
  // The accounts that do have an entry are unaffected.
  EXPECT_TRUE(find(build(true), "root")->shadow_error.empty());
}

TEST(local_accounts_linux, gather_from_reads_the_files_and_reports_an_unreadable_passwd) {
  temp_dir t;
  const std::string broken = "broken:x:notanumber:1:::\n";
  t.write("passwd", passwd_fixture.substr(0, passwd_fixture.size() - broken.size()));
  t.write("shadow", shadow_fixture, 0600);
  std::vector<local_accounts_filter::filter_obj_ptr> out;
  std::string error;
  local_accounts_source::gather_from(t.path() + "/passwd", t.path() + "/shadow", now, out, error);
  ASSERT_TRUE(error.empty()) << error;
  EXPECT_EQ(12u, out.size());
  EXPECT_EQ("set", find(out, "root")->password_state);

  // A missing shadow is a per-row contract, a missing passwd fails the check.
  out.clear();
  local_accounts_source::gather_from(t.path() + "/passwd", t.path() + "/nope", now, out, error);
  ASSERT_TRUE(error.empty()) << error;
  EXPECT_NE(find(out, "root")->shadow_error.find("Cannot read " + t.path() + "/nope"), std::string::npos);

  out.clear();
  local_accounts_source::gather_from(t.path() + "/nope", t.path() + "/shadow", now, out, error);
  EXPECT_TRUE(out.empty());
  EXPECT_NE(error.find("Cannot read"), std::string::npos) << error;
}

// ---------------------------------------------------------------------------
// check_group_members
// ---------------------------------------------------------------------------

namespace {

const std::string group_fixture =
    "root:x:0:\n"
    "adm:x:4:syslog,alice\n"
    "wheel:x:10:alice\n"
    "sudo:x:27:alice,ghost,Alice\n"
    "users:x:100:\n"
    "admins:x:1010:\n"
    "+:::\n";

const std::string member_passwd =
    "root:x:0:0:root:/root:/bin/bash\n"
    "alice:x:1000:1000::/home/alice:/bin/bash\n"
    "Alice:x:1001:1001::/home/Alice:/bin/bash\n"
    "carol:x:1002:27::/home/carol:/bin/bash\n"
    "dave:x:1003:1010::/home/dave:/bin/bash\n";

std::vector<group_members_filter::filter_obj_ptr> members_of(const std::string &group, std::string &error) {
  std::vector<group_members_filter::filter_obj_ptr> out;
  group_members_source::members_from(group_fixture, member_passwd, group, out, error);
  return out;
}

}  // namespace

TEST(group_members_linux, the_default_group_is_the_first_of_sudo_and_wheel) {
  EXPECT_EQ("sudo", group_members_source::first_existing_group(group_fixture, {"sudo", "wheel"}));
  EXPECT_EQ("wheel", group_members_source::first_existing_group("root:x:0:\nwheel:x:10:alice\n", {"sudo", "wheel"}));
  EXPECT_EQ("", group_members_source::first_existing_group("root:x:0:\n", {"sudo", "wheel"}));
}

TEST(group_members_linux, listed_and_primary_members_are_both_reported) {
  std::string error;
  const auto rows = members_of("sudo", error);
  ASSERT_TRUE(error.empty()) << error;
  std::vector<std::string> names;
  for (const auto &r : rows) names.push_back(r->member);
  // Listed first, in file order; then carol, whose primary gid is 27.
  EXPECT_EQ((std::vector<std::string>{"alice", "ghost", "Alice", "carol"}), names);

  EXPECT_EQ("sudo", rows[0]->group);
  EXPECT_EQ("alice", rows[0]->name);
  EXPECT_EQ("", rows[0]->domain);
  EXPECT_EQ("1000", rows[0]->sid);
  EXPECT_EQ("user", rows[0]->type);
  EXPECT_TRUE(rows[0]->case_sensitive);
  // A listed name with no local account: a directory user or a stale entry.
  EXPECT_EQ("unknown", rows[1]->type);
  EXPECT_EQ("", rows[1]->sid);
  EXPECT_EQ("1002", rows[3]->sid);
}

TEST(group_members_linux, a_member_in_both_places_is_reported_once) {
  std::string error;
  std::vector<group_members_filter::filter_obj_ptr> out;
  group_members_source::members_from("staff:x:50:alice\n", "alice:x:1000:50::/home/alice:/bin/bash\n", "staff", out, error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ("alice", out[0]->member);
}

TEST(group_members_linux, a_group_split_over_several_lines_lists_every_line) {
  // initgroups(3) reads every line, so the second one grants membership too.
  std::string error;
  std::vector<group_members_filter::filter_obj_ptr> out;
  group_members_source::members_from("sudo:x:27:alice\nusers:x:100:\nsudo:x:27:mallory,alice\n", member_passwd, "sudo", out, error);
  ASSERT_TRUE(error.empty()) << error;
  std::vector<std::string> names;
  for (const auto &r : out) names.push_back(r->member);
  EXPECT_EQ((std::vector<std::string>{"alice", "mallory", "carol"}), names);
}

TEST(group_members_linux, a_differently_named_line_with_the_same_gid_confers_membership) {
  std::string error;
  std::vector<group_members_filter::filter_obj_ptr> out;
  group_members_source::members_from("sudo:x:27:alice\nhelpers:x:27:mallory\nother:x:28:bob\n", member_passwd, "sudo", out, error);
  ASSERT_TRUE(error.empty()) << error;
  std::vector<std::string> names;
  for (const auto &r : out) names.push_back(r->member);
  EXPECT_EQ((std::vector<std::string>{"alice", "mallory", "carol"}), names);
  EXPECT_EQ("sudo", out[1]->group);
}

TEST(group_members_linux, a_malformed_line_is_an_error_not_a_missing_group) {
  for (const std::string bad : {"sudo:x:27:alice:\n", "sudo:x:abc:alice\n", "sudo:x::alice\n", "sudo:x\n", "sudo:x:+27:alice\n"}) {
    std::string error;
    std::vector<group_members_filter::filter_obj_ptr> out;
    group_members_source::members_from(bad + "wheel:x:10:alice\n", member_passwd, "sudo", out, error);
    EXPECT_TRUE(out.empty()) << bad;
    EXPECT_NE(error.find("Malformed entry for group sudo"), std::string::npos) << error;
    // The default must not fall through to wheel and answer for it instead.
    EXPECT_EQ("sudo", group_members_source::first_existing_group(bad + "wheel:x:10:alice\n", {"sudo", "wheel"})) << bad;
  }
}

TEST(group_members_linux, a_malformed_line_for_another_group_is_an_error_too) {
  // Membership is granted by gid, so a damaged line under another name - or
  // under none - may be one the system reads as granting this group. Listing
  // the members that could be read would hide the ones on it.
  for (const std::string bad : {"admins:x:27:mallory:junk\n", ":x:27:mallory\n", "adm:x:abc:syslog\n", "adm:x\n"}) {
    std::string error;
    std::vector<group_members_filter::filter_obj_ptr> out;
    group_members_source::members_from("sudo:x:27:alice\n" + bad, member_passwd, "sudo", out, error);
    EXPECT_TRUE(out.empty()) << bad;
    EXPECT_NE(error.find("1 unparseable line(s) in /etc/group (line 2)"), std::string::npos) << bad << error;
    EXPECT_NE(error.find("grpck"), std::string::npos) << error;
  }
  // The damage is reported rather than a "not found" for a group that may be on it.
  std::string error;
  std::vector<group_members_filter::filter_obj_ptr> out;
  group_members_source::members_from("root:x:0:\n:x:27:mallory\n", member_passwd, "sudo", out, error);
  EXPECT_NE(error.find("unparseable line(s) in /etc/group"), std::string::npos) << error;
}

TEST(group_members_linux, a_group_line_without_a_member_field_is_an_empty_group) {
  // glibc returns "sudo:x:27" as a group with no listed members.
  std::string error;
  std::vector<group_members_filter::filter_obj_ptr> out;
  group_members_source::members_from("  sudo:x:27\n", member_passwd, "sudo", out, error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ("carol", out[0]->member);  // through her primary gid
}

TEST(group_members_linux, an_unparseable_passwd_line_is_an_error) {
  // It may be an account whose primary group this is.
  std::string error;
  std::vector<group_members_filter::filter_obj_ptr> out;
  group_members_source::members_from("sudo:x:27:alice\n", "alice:x:1000:1000::/home/alice:/bin/bash\nmallory:x:+1:27::/:/bin/sh\n", "sudo", out, error);
  EXPECT_TRUE(out.empty());
  EXPECT_NE(error.find("1 unparseable line(s) in /etc/passwd (line 2)"), std::string::npos) << error;
}

TEST(group_members_linux, a_group_with_only_primary_members_is_not_empty) {
  std::string error;
  const auto rows = members_of("admins", error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ("dave", rows[0]->member);
}

TEST(group_members_linux, an_empty_group_has_no_rows) {
  std::string error;
  EXPECT_TRUE(members_of("users", error).empty());
  EXPECT_TRUE(error.empty()) << error;
}

TEST(group_members_linux, an_unknown_group_is_an_error) {
  std::string error;
  EXPECT_TRUE(members_of("Administrators", error).empty());
  EXPECT_EQ("Local group not found: Administrators", error);
}

TEST(group_members_linux, no_default_group_is_an_error_that_says_what_to_do) {
  std::string error;
  EXPECT_TRUE(members_of("", error).empty());
  EXPECT_NE(error.find("pass group="), std::string::npos) << error;
}

TEST(group_members_linux, the_allow_list_is_case_sensitive_on_linux) {
  std::string error;
  const auto rows = members_of("sudo", error);
  ASSERT_EQ(4u, rows.size());
  EXPECT_TRUE(group_members_filter::is_expected(*rows[0], {"alice"}));
  EXPECT_FALSE(group_members_filter::is_expected(*rows[2], {"alice"}));  // "Alice" is another account
}

TEST(group_members_linux, gather_from_reads_the_files) {
  temp_dir t;
  t.write("group", group_fixture);
  t.write("passwd", member_passwd);
  std::vector<group_members_filter::filter_obj_ptr> out;
  std::string error;
  group_members_source::gather_from(t.path() + "/group", t.path() + "/passwd", "wheel", out, error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ("alice", out[0]->member);

  out.clear();
  group_members_source::gather_from(t.path() + "/nope", t.path() + "/passwd", "wheel", out, error);
  EXPECT_NE(error.find("Cannot read"), std::string::npos) << error;
}
