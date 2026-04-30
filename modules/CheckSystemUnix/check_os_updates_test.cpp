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

#include "check_os_updates.h"

#include <gtest/gtest.h>

using os_updates::filter_obj;

// ============================================================================
// filter_obj basic behavior
// ============================================================================

TEST(CheckOsUpdates, filter_obj_defaults) {
  filter_obj obj;
  EXPECT_EQ(obj.count, 0);
  EXPECT_EQ(obj.security, 0);
  EXPECT_EQ(obj.manager, "");
  EXPECT_TRUE(obj.packages.empty());
  EXPECT_EQ(obj.show(), "no updates available");
}

TEST(CheckOsUpdates, filter_obj_show_with_updates) {
  filter_obj obj;
  obj.count = 5;
  obj.security = 2;
  EXPECT_EQ(obj.show(), "5 updates available (2 security)");
}

TEST(CheckOsUpdates, filter_obj_show_no_security) {
  filter_obj obj;
  obj.count = 3;
  obj.security = 0;
  EXPECT_EQ(obj.show(), "3 updates available");
}

TEST(CheckOsUpdates, filter_obj_packages_string) {
  filter_obj obj;
  obj.packages.push_back(os_updates::package_update("a", "1", "src", false));
  obj.packages.push_back(os_updates::package_update("b", "2", "src", true));
  EXPECT_EQ(obj.get_packages(), "a, b");
}

// ============================================================================
// apt parser
// ============================================================================

TEST(CheckOsUpdates, parse_apt_empty) {
  filter_obj obj = os_updates::parse_apt_output("");
  EXPECT_EQ(obj.manager, "apt");
  EXPECT_EQ(obj.count, 0);
  EXPECT_EQ(obj.security, 0);
}

TEST(CheckOsUpdates, parse_apt_listing_only) {
  filter_obj obj = os_updates::parse_apt_output("Listing... Done\n");
  EXPECT_EQ(obj.count, 0);
}

TEST(CheckOsUpdates, parse_apt_basic) {
  std::string output =
      "Listing... Done\n"
      "curl/jammy-updates,jammy-updates 7.81.0-1ubuntu1.16 amd64 [upgradable from: 7.81.0-1ubuntu1.15]\n"
      "openssl/jammy-security,jammy-security 3.0.2-0ubuntu1.18 amd64 [upgradable from: 3.0.2-0ubuntu1.17]\n"
      "python3/jammy-updates,jammy-updates 3.10.6-1~22.04 amd64 [upgradable from: 3.10.4-0ubuntu2]\n";
  filter_obj obj = os_updates::parse_apt_output(output);
  EXPECT_EQ(obj.manager, "apt");
  EXPECT_EQ(obj.count, 3);
  EXPECT_EQ(obj.security, 1);
  ASSERT_EQ(obj.packages.size(), 3u);
  EXPECT_EQ(obj.packages[0].name, "curl");
  EXPECT_EQ(obj.packages[0].version, "7.81.0-1ubuntu1.16");
  EXPECT_FALSE(obj.packages[0].security);
  EXPECT_EQ(obj.packages[1].name, "openssl");
  EXPECT_TRUE(obj.packages[1].security);
  EXPECT_EQ(obj.packages[2].name, "python3");
  EXPECT_FALSE(obj.packages[2].security);
}

TEST(CheckOsUpdates, parse_apt_skips_warnings) {
  std::string output =
      "WARNING: apt does not have a stable CLI interface.\n"
      "Listing...\n"
      "vim/jammy-security 9.0 amd64 [upgradable from: 8.2]\n";
  filter_obj obj = os_updates::parse_apt_output(output);
  EXPECT_EQ(obj.count, 1);
  EXPECT_EQ(obj.security, 1);
}

// ============================================================================
// dnf/yum parser
// ============================================================================

TEST(CheckOsUpdates, parse_dnf_empty) {
  filter_obj obj = os_updates::parse_dnf_output("");
  EXPECT_EQ(obj.manager, "dnf");
  EXPECT_EQ(obj.count, 0);
}

TEST(CheckOsUpdates, parse_dnf_basic) {
  std::string output =
      "Last metadata expiration check: 0:10:42 ago on Mon Jan 01 12:00:00 2024.\n"
      "\n"
      "kernel.x86_64                  5.14.0-362.18.1.el9_3   baseos\n"
      "openssl.x86_64                 1:3.0.7-25.el9_3        rhel-9-security\n"
      "vim-common.x86_64              2:8.2.2637-20.el9_1     appstream\n";
  filter_obj obj = os_updates::parse_dnf_output(output);
  EXPECT_EQ(obj.manager, "dnf");
  EXPECT_EQ(obj.count, 3);
  EXPECT_EQ(obj.security, 1);
  ASSERT_EQ(obj.packages.size(), 3u);
  EXPECT_EQ(obj.packages[0].name, "kernel");
  EXPECT_EQ(obj.packages[0].version, "5.14.0-362.18.1.el9_3");
  EXPECT_EQ(obj.packages[0].source, "baseos");
  EXPECT_FALSE(obj.packages[0].security);
  EXPECT_EQ(obj.packages[1].name, "openssl");
  EXPECT_TRUE(obj.packages[1].security);
}

TEST(CheckOsUpdates, parse_dnf_skips_obsoletes_section) {
  std::string output =
      "kernel.x86_64                  5.14.0-362.18.1.el9_3   baseos\n"
      "Obsoleting Packages\n"
      "  oldpkg.x86_64                  1.0-1.el9               baseos\n"
      "      replacing oldpkg-thing.x86_64 0.9-1.el9\n";
  filter_obj obj = os_updates::parse_dnf_output(output);
  EXPECT_EQ(obj.count, 1);
  ASSERT_EQ(obj.packages.size(), 1u);
  EXPECT_EQ(obj.packages[0].name, "kernel");
}

// ============================================================================
// zypper parser
// ============================================================================

TEST(CheckOsUpdates, parse_zypper_basic) {
  std::string output =
      "S | Repository                 | Name        | Current Version | Available Version | Arch\n"
      "--+----------------------------+-------------+-----------------+-------------------+------\n"
      "v | SLES15-SP4-Updates         | curl        | 7.79.1-150400.4 | 7.79.1-150400.5   | x86_64\n"
      "v | SLES15-SP4-Security        | openssl     | 1.1.1l-150400.7 | 1.1.1l-150400.8   | x86_64\n";
  filter_obj obj = os_updates::parse_zypper_output(output);
  EXPECT_EQ(obj.manager, "zypper");
  EXPECT_EQ(obj.count, 2);
  // Both rows have repo names containing "Updates" or "Security" -> security flag set
  EXPECT_EQ(obj.security, 2);
  ASSERT_EQ(obj.packages.size(), 2u);
  EXPECT_EQ(obj.packages[0].name, "curl");
  EXPECT_EQ(obj.packages[1].name, "openssl");
}

TEST(CheckOsUpdates, parse_zypper_empty) {
  filter_obj obj = os_updates::parse_zypper_output("");
  EXPECT_EQ(obj.count, 0);
}

// ============================================================================
// pacman parser
// ============================================================================

TEST(CheckOsUpdates, parse_pacman_basic) {
  std::string output =
      "linux 6.6.7.arch1-1 -> 6.6.8.arch1-1\n"
      "openssl 3.2.0-1 -> 3.2.0-2\n";
  filter_obj obj = os_updates::parse_pacman_output(output);
  EXPECT_EQ(obj.manager, "pacman");
  EXPECT_EQ(obj.count, 2);
  EXPECT_EQ(obj.security, 0);  // pacman has no security distinction
  ASSERT_EQ(obj.packages.size(), 2u);
  EXPECT_EQ(obj.packages[0].name, "linux");
  EXPECT_EQ(obj.packages[0].version, "6.6.8.arch1-1");
  EXPECT_EQ(obj.packages[1].name, "openssl");
  EXPECT_EQ(obj.packages[1].version, "3.2.0-2");
}

TEST(CheckOsUpdates, parse_pacman_empty) {
  filter_obj obj = os_updates::parse_pacman_output("");
  EXPECT_EQ(obj.count, 0);
}

// ============================================================================
// fetch_updates dispatch with an injected exec function
// ============================================================================

TEST(CheckOsUpdates, fetch_updates_unknown_manager) {
  filter_obj obj = os_updates::fetch_updates("", [](const std::string &) -> std::string { return ""; });
  EXPECT_EQ(obj.manager, "none");
  EXPECT_EQ(obj.count, 0);
}

TEST(CheckOsUpdates, fetch_updates_dispatches_to_apt) {
  std::string captured_cmd;
  auto fake_exec = [&](const std::string &cmd) -> std::string {
    captured_cmd = cmd;
    return "Listing...\nvim/jammy-security 9.0 amd64 [upgradable from: 8.2]\n";
  };
  filter_obj obj = os_updates::fetch_updates("apt", fake_exec);
  EXPECT_NE(captured_cmd.find("apt list"), std::string::npos);
  EXPECT_EQ(obj.manager, "apt");
  EXPECT_EQ(obj.count, 1);
  EXPECT_EQ(obj.security, 1);
}

TEST(CheckOsUpdates, fetch_updates_dispatches_to_yum) {
  auto fake_exec = [](const std::string &) -> std::string { return "kernel.x86_64                  5.14.0-1   baseos\n"; };
  filter_obj obj = os_updates::fetch_updates("yum", fake_exec);
  EXPECT_EQ(obj.manager, "yum");
  EXPECT_EQ(obj.count, 1);
}

TEST(CheckOsUpdates, fetch_updates_dispatches_to_pacman) {
  auto fake_exec = [](const std::string &) -> std::string { return "linux 6.0 -> 6.1\n"; };
  filter_obj obj = os_updates::fetch_updates("pacman", fake_exec);
  EXPECT_EQ(obj.manager, "pacman");
  EXPECT_EQ(obj.count, 1);
}

// ============================================================================
// detect_manager - smoke test (doesn't assert any specific manager since the
// runner environment varies).
// ============================================================================

TEST(CheckOsUpdates, detect_manager_returns_known_or_empty) {
  std::string mgr = os_updates::detect_manager();
  if (!mgr.empty()) {
    EXPECT_TRUE(mgr == "apt" || mgr == "dnf" || mgr == "yum" || mgr == "zypper" || mgr == "pacman") << "got: " << mgr;
  }
}
