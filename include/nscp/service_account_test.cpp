// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscp/service_account.hpp>
#include <string>
#include <vector>

namespace sa = nscp::service_account;

// The SID is what ends up in an ACL, so it has to be the one Windows derives
// for the service - not one that merely looks like it. These are fixed points
// taken from Windows itself.
TEST(ServiceAccount, SidMatchesTheOnesWindowsPublishes) {
  // The documented, well-known SID of NT SERVICE\TrustedInstaller.
  EXPECT_EQ(sa::service_sid("TrustedInstaller"), "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464");
  // NT SERVICE\WinDefend, as it appears in the ACLs of any Windows host.
  EXPECT_EQ(sa::service_sid("WinDefend"), "S-1-5-80-1913148863-3492339771-4165695881-2087618961-4109116736");
}

TEST(ServiceAccount, SidIgnoresTheCaseOfTheName) {
  // Service names are case-insensitive, and Windows hashes the upper-cased
  // name: `nscp` and `NSCP` are one service and must be one SID.
  EXPECT_EQ(sa::service_sid("nscp"), sa::service_sid("NSCP"));
  EXPECT_EQ(sa::service_sid("trustedinstaller"), sa::service_sid("TrustedInstaller"));
  EXPECT_NE(sa::service_sid("nscp"), sa::service_sid("nscp2"));
}

TEST(ServiceAccount, Sha1HandlesInputAcrossABlockBoundary) {
  // 28 characters is 56 bytes of UTF-16, which pushes the length field into a
  // second block - the padding case a short service name never reaches.
  const std::string long_name(28, 'a');
  const std::string longer_name(40, 'a');
  EXPECT_EQ(sa::service_sid(long_name).compare(0, 9, "S-1-5-80-"), 0);
  EXPECT_NE(sa::service_sid(long_name), sa::service_sid(longer_name));

  // And the hash itself, against the standard test vectors.
  unsigned char digest[20];
  const std::string abc = "abc";
  sa::detail::sha1(std::vector<unsigned char>(abc.begin(), abc.end()), digest);
  EXPECT_EQ(digest[0], 0xA9);
  EXPECT_EQ(digest[1], 0x99);
  EXPECT_EQ(digest[19], 0x9D);
  const std::string two_blocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  sa::detail::sha1(std::vector<unsigned char>(two_blocks.begin(), two_blocks.end()), digest);
  EXPECT_EQ(digest[0], 0x84);
  EXPECT_EQ(digest[1], 0x98);
  EXPECT_EQ(digest[19], 0xF1);
}

TEST(ServiceAccount, NamesThatCannotBeHashedFaithfullyAreRefused) {
  EXPECT_TRUE(sa::is_supported_service_name("nscp"));
  EXPECT_TRUE(sa::is_supported_service_name("NSClient++"));
  EXPECT_FALSE(sa::is_supported_service_name(""));
  EXPECT_FALSE(sa::is_supported_service_name("my service"));
  EXPECT_FALSE(sa::is_supported_service_name("a\\b"));
  EXPECT_FALSE(sa::is_supported_service_name("a/b"));
  // Upper-casing this needs the kernel's case table, not ours.
  EXPECT_FALSE(sa::is_supported_service_name("\xC3\xB6vervakning"));
  EXPECT_EQ(sa::service_sid("my service"), "");
}

TEST(ServiceAccount, AnUnknownAccountKindIsLocalSystem) {
  EXPECT_EQ(sa::parse("virtual"), sa::kind::virtual_account);
  EXPECT_EQ(sa::parse("system"), sa::kind::system);
  EXPECT_EQ(sa::parse(""), sa::kind::system);
  // A typo must not start granting access to an account nobody chose.
  EXPECT_EQ(sa::parse("Virtual"), sa::kind::system);
  EXPECT_EQ(sa::parse("virtuel"), sa::kind::system);
  EXPECT_TRUE(sa::is_known(""));
  EXPECT_TRUE(sa::is_known("virtual"));
  EXPECT_FALSE(sa::is_known("virtuel"));
}

TEST(ServiceAccount, OnlyAVirtualAccountAddsAPrincipal) {
  sa::identity who;
  who.service_name = "nscp";
  EXPECT_TRUE(sa::acl_principals(who).empty());
  EXPECT_EQ(sa::account_name(who), "LocalSystem");

  who.account = sa::kind::virtual_account;
  ASSERT_EQ(sa::acl_principals(who).size(), 1u);
  EXPECT_EQ(sa::acl_principals(who).front(), sa::service_sid("nscp"));
  EXPECT_EQ(sa::account_name(who), "NT SERVICE\\nscp");

  // A record without a usable name grants nobody anything.
  who.service_name = "";
  EXPECT_TRUE(sa::acl_principals(who).empty());
}

TEST(ServiceAccount, TheVirtualAccountIsRefusedOnTheLegacyLayout) {
  using nscp::paths::layout;
  EXPECT_EQ(sa::refusal(sa::kind::system, "nscp", layout::legacy), "");
  EXPECT_EQ(sa::refusal(sa::kind::system, "nscp", layout::modern), "");
  EXPECT_EQ(sa::refusal(sa::kind::virtual_account, "nscp", layout::modern), "");

  const std::string why = sa::refusal(sa::kind::virtual_account, "nscp", layout::legacy);
  ASSERT_FALSE(why.empty());
  // The message is the whole answer the operator gets: it has to say what to
  // do, both ways of doing it.
  EXPECT_NE(why.find("nscp settings --migrate-layout modern"), std::string::npos) << why;
  EXPECT_NE(why.find("LAYOUT=modern"), std::string::npos) << why;

  EXPECT_FALSE(sa::refusal(sa::kind::virtual_account, "my service", layout::modern).empty());
}

TEST(ServiceAccount, LeavingTheModernLayoutIsRefusedWhileTheAccountIsVirtual) {
  using nscp::paths::layout;
  sa::identity who;
  who.service_name = "nscp";
  EXPECT_EQ(sa::layout_change_refusal(who, layout::legacy), "");

  who.account = sa::kind::virtual_account;
  EXPECT_EQ(sa::layout_change_refusal(who, layout::modern), "");
  const std::string why = sa::layout_change_refusal(who, layout::legacy);
  ASSERT_FALSE(why.empty());
  EXPECT_NE(why.find("nscp service --account system"), std::string::npos) << why;
}

TEST(ServiceAccount, ImpersonationIsNotAmongTheRequiredPrivileges) {
  // The reason the list exists: a service account that keeps
  // SeImpersonatePrivilege is one step from SYSTEM.
  for (const std::string &privilege : sa::required_privileges()) {
    EXPECT_NE(privilege, "SeImpersonatePrivilege");
    EXPECT_NE(privilege, "SeAssignPrimaryTokenPrivilege");
    EXPECT_NE(privilege, "SeDebugPrivilege");
    EXPECT_NE(privilege, "SeTcbPrivilege");
  }
  EXPECT_FALSE(sa::required_privileges().empty());
}

TEST(ServiceAccount, ReadsTheIdentityFromBootIni) {
  CSimpleIni boot_conf;
  ASSERT_GE(boot_conf.LoadData("[service]\naccount = virtual\nname = nscp\n"), 0);
  const sa::identity who = sa::from_boot_ini(boot_conf);
  EXPECT_EQ(who.account, sa::kind::virtual_account);
  EXPECT_EQ(who.service_name, "nscp");

  CSimpleIni empty;
  ASSERT_GE(empty.LoadData("[layout]\nmode = modern\n"), 0);
  EXPECT_EQ(sa::from_boot_ini(empty).account, sa::kind::system);

  // No boot.ini at all is every installation that predates the setting.
  EXPECT_EQ(sa::from_boot_ini_file("/no/such/boot.ini").account, sa::kind::system);
}
