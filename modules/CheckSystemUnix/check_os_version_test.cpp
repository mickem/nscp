// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_os_version.h"

#include <gtest/gtest.h>

using os_version::parse_os_release;

TEST(CheckOsVersion, ParsesUbuntuOsRelease) {
  const std::string content =
      "PRETTY_NAME=\"Ubuntu 22.04.3 LTS\"\n"
      "NAME=\"Ubuntu\"\n"
      "VERSION_ID=\"22.04\"\n"
      "VERSION=\"22.04.3 LTS (Jammy Jellyfish)\"\n"
      "ID=ubuntu\n"
      "ID_LIKE=debian\n"
      "HOME_URL=\"https://www.ubuntu.com/\"\n";
  const auto r = parse_os_release(content);
  EXPECT_EQ(r.distribution, "ubuntu");
  EXPECT_EQ(r.distribution_name, "Ubuntu");
  EXPECT_EQ(r.version, "22.04");
  EXPECT_EQ(r.family, "debian");
  EXPECT_EQ(r.pretty, "Ubuntu 22.04.3 LTS");
}

TEST(CheckOsVersion, IdLikeTakesFirstToken) {
  const auto r = parse_os_release("ID=rocky\nID_LIKE=\"rhel centos fedora\"\n");
  EXPECT_EQ(r.distribution, "rocky");
  EXPECT_EQ(r.family, "rhel");
}

TEST(CheckOsVersion, FamilyFallsBackToId) {
  const auto r = parse_os_release("ID=debian\nVERSION_ID=\"12\"\n");
  EXPECT_EQ(r.family, "debian");  // no ID_LIKE -> family is the distro itself
  EXPECT_EQ(r.version, "12");
}

TEST(CheckOsVersion, IgnoresCommentsAndBlankLines) {
  const auto r = parse_os_release("# a comment\n\nID=alpine\n\n# another\nVERSION_ID=3.19\n");
  EXPECT_EQ(r.distribution, "alpine");
  EXPECT_EQ(r.version, "3.19");
}

TEST(CheckOsVersion, EmptyContentYieldsEmptyStruct) {
  const auto r = parse_os_release("");
  EXPECT_TRUE(r.distribution.empty());
  EXPECT_TRUE(r.pretty.empty());
  EXPECT_TRUE(r.family.empty());
}

TEST(CheckOsVersion, SingleQuotedValues) {
  const auto r = parse_os_release("ID='fedora'\nVERSION_ID='40'\n");
  EXPECT_EQ(r.distribution, "fedora");
  EXPECT_EQ(r.version, "40");
}
