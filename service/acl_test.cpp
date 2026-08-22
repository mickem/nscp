// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#ifdef WIN32

#include <windows.h>
// windows.h has to come first; aclapi/sddl depend on its types.
#include <aclapi.h>
#include <sddl.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <list>
#include <string>
#include <str/utf8.hpp>
#include <vector>
#include <win/acl.hpp>

// The lockdown behind the modern layout's shared folder.
//
// It matters most when it silently does not happen: %ProgramData% grants
// `Users: Read & Execute` by inheritance, so a folder created there and left
// alone holds the configuration (passwords) and the fleet identity's private
// key readable by every account on the machine, with nothing else looking
// wrong. Adding two explicit ACEs is not enough on its own - the inherited one
// survives beside them unless inheritance is explicitly broken.
class AclTest : public ::testing::Test {
 protected:
  boost::filesystem::path dir_;

  void SetUp() override {
    dir_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-acl-%%%%-%%%%");
    boost::filesystem::create_directories(dir_);
  }
  void TearDown() override {
    boost::system::error_code ignored;
    // Undo the protection first: a test that locked the directory to SYSTEM and
    // Administrators may not be able to delete it as an ordinary user.
    if (protected_) restore();
    boost::filesystem::remove_all(dir_, ignored);
  }

  // Hand the directory back to inheritance so the fixture can clean up.
  // Ownership has to come back first: protect_directory hands the folder to
  // Administrators, and without it a /reset can be refused on the way out.
  void restore() {
    const std::string take_back = "icacls \"" + dir_.string() + "\" /setowner \"%USERNAME%\" /t /q > nul 2>&1";
    const std::string reset = "icacls \"" + dir_.string() + "\" /reset /t /q > nul 2>&1";
    for (const std::string &command : {take_back, reset}) {
      const int ignored_result = std::system(command.c_str());
      static_cast<void>(ignored_result);
    }
  }

  bool protected_ = false;

  // The SID of the account running the test, kept alive in `storage`.
  static PSID current_user(std::vector<unsigned char> &storage) {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return nullptr;
    DWORD size = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    storage.assign(size, 0);
    const bool got = size != 0 && ::GetTokenInformation(token, TokenUser, storage.data(), size, &size);
    ::CloseHandle(token);
    if (!got) return nullptr;
    return reinterpret_cast<const TOKEN_USER *>(storage.data())->User.Sid;
  }

  static PSID well_known(const WELL_KNOWN_SID_TYPE type, std::vector<unsigned char> &storage) {
    DWORD size = SECURITY_MAX_SID_SIZE;
    storage.assign(size, 0);
    if (!::CreateWellKnownSid(type, nullptr, storage.data(), &size)) return nullptr;
    return storage.data();
  }

  // The directory's current owner, kept alive in `descriptor`.
  PSID owner_of(PSECURITY_DESCRIPTOR &descriptor) const {
    PSID owner = nullptr;
    const std::wstring wide = utf8::cvt<std::wstring>(dir_.string());
    if (::GetNamedSecurityInfoW(wide.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr, nullptr, &descriptor) != ERROR_SUCCESS) {
      return nullptr;
    }
    return owner;
  }

  // Hand ownership to the account running the test without touching the DACL.
  // This reproduces what pre-creating the folder leaves behind: ACEs that look
  // right, under an owner who keeps implicit WRITE_DAC.
  bool give_ownership_to_current_user() {
    std::vector<unsigned char> storage;
    PSID user = current_user(storage);
    if (user == nullptr) return false;
    const std::wstring wide = utf8::cvt<std::wstring>(dir_.string());
    return ::SetNamedSecurityInfoW(const_cast<LPWSTR>(wide.c_str()), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, user, nullptr, nullptr, nullptr) ==
           ERROR_SUCCESS;
  }

  // True when the test runs as SYSTEM or with Administrators as its owner SID,
  // in which case "hand it to the current user" is not a foreign owner at all
  // and the test below would be asserting nothing.
  bool current_user_is_a_trusted_owner() {
    std::vector<unsigned char> user_storage, system_storage, admin_storage;
    PSID user = current_user(user_storage);
    PSID system = well_known(WinLocalSystemSid, system_storage);
    PSID admin = well_known(WinBuiltinAdministratorsSid, admin_storage);
    if (user == nullptr || system == nullptr || admin == nullptr) return true;
    return ::EqualSid(user, system) || ::EqualSid(user, admin);
  }
};

TEST_F(AclTest, ATemporaryDirectoryIsNotProtectedToStartWith) {
  // The precondition the rest of this rests on: a directory created the normal
  // way inherits access for principals we do not want. If this ever stops being
  // true the other test would pass for the wrong reason.
  std::list<std::string> errors;
  EXPECT_FALSE(nsclient::windows_acl::is_protected(dir_.string(), errors));
  EXPECT_FALSE(errors.empty()) << "is_protected should say who else has access";
}

TEST_F(AclTest, ProtectingBreaksInheritanceAndExcludesEveryoneElse) {
  std::list<std::string> errors;
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
  protected_ = true;

  errors.clear();
  EXPECT_TRUE(nsclient::windows_acl::is_protected(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
  EXPECT_TRUE(errors.empty());
}

TEST_F(AclTest, ProtectingIsIdempotent) {
  // Applied at every boot, so running it twice has to be a no-op rather than
  // an accumulation of ACEs.
  std::list<std::string> errors;
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors));
  protected_ = true;
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors));
  EXPECT_TRUE(nsclient::windows_acl::is_protected(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
}

TEST_F(AclTest, ProtectingTakesOwnership) {
  // Without this the two ACEs are advisory: %ProgramData% hands
  // `CREATOR OWNER: Full` to whoever creates the folder, and an owner keeps
  // implicit WRITE_DAC whatever the DACL says.
  std::list<std::string> errors;
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
  protected_ = true;

  PSECURITY_DESCRIPTOR raw_descriptor = nullptr;
  const PSID owner = owner_of(raw_descriptor);
  ASSERT_NE(owner, nullptr);
  std::vector<unsigned char> admin_storage;
  const PSID admin = well_known(WinBuiltinAdministratorsSid, admin_storage);
  ASSERT_NE(admin, nullptr);
  EXPECT_TRUE(::EqualSid(owner, admin)) << "the folder is not owned by Administrators after protecting it";
  if (raw_descriptor != nullptr) ::LocalFree(raw_descriptor);
}

TEST_F(AclTest, ACleanDaclUnderAForeignOwnerIsNotProtected) {
  // The state an attacker gets by pre-creating C:\ProgramData\NSClient++: the
  // ACEs are exactly what we want, but the owner can rewrite them at will. If
  // is_protected only looked at the DACL this would read as locked down.
  if (current_user_is_a_trusted_owner()) {
    GTEST_SKIP() << "running as SYSTEM or Administrators, so there is no foreign owner to test with";
  }
  std::list<std::string> errors;
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
  protected_ = true;
  ASSERT_TRUE(give_ownership_to_current_user());

  errors.clear();
  EXPECT_FALSE(nsclient::windows_acl::is_protected(dir_.string(), errors));
  EXPECT_FALSE(errors.empty()) << "is_protected should say who owns it";
}

TEST_F(AclTest, ReprotectingTakesOwnershipBack) {
  // Re-applied at every boot, which is what makes a folder someone pre-created
  // recoverable rather than permanently theirs.
  if (current_user_is_a_trusted_owner()) {
    GTEST_SKIP() << "running as SYSTEM or Administrators, so there is no foreign owner to test with";
  }
  std::list<std::string> errors;
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors));
  protected_ = true;
  ASSERT_TRUE(give_ownership_to_current_user());
  ASSERT_FALSE(nsclient::windows_acl::is_protected(dir_.string(), errors));

  errors.clear();
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
  EXPECT_TRUE(nsclient::windows_acl::is_protected(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
}

TEST_F(AclTest, AnUnreadableSecurityDescriptorIsUnknownRatherThanOpen) {
  // The distinction the shared-folder bootstrap depends on. Reading a security
  // descriptor needs READ_CONTROL, which the lockdown denies to everyone else,
  // so an unelevated caller looking at a *correctly* protected folder cannot
  // read it. Answering "open" there produced a frightening warning on every
  // `nscp client ...` about a folder that was exactly right.
  std::list<std::string> errors;
  const std::string missing = (dir_ / "no-such-folder").string();
  EXPECT_EQ(nsclient::windows_acl::inspect_protection(missing, errors), nsclient::windows_acl::protection::unknown);
  EXPECT_FALSE(errors.empty());

  // A directory we *can* read and that grants others access is a different
  // answer, and the one worth warning about.
  errors.clear();
  EXPECT_EQ(nsclient::windows_acl::inspect_protection(dir_.string(), errors), nsclient::windows_acl::protection::open);
}

TEST_F(AclTest, InspectionAgreesWithIsProtected) {
  std::list<std::string> errors;
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
  protected_ = true;
  errors.clear();
  EXPECT_EQ(nsclient::windows_acl::inspect_protection(dir_.string(), errors), nsclient::windows_acl::protection::restricted);
  EXPECT_TRUE(nsclient::windows_acl::is_protected(dir_.string(), errors));
}

TEST_F(AclTest, ResettingMakesARenamedTreeInheritTheDestination) {
  // The layout migration moves state with a same-volume rename, which keeps
  // each entry's old security descriptor: a file that was world-readable in
  // Program Files is still world-readable inside the locked-down folder, while
  // the folder itself claims otherwise. reset_to_inherited() on the renamed
  // entry has to fix that - including everything *inside* a renamed directory,
  // which is how fleet\ (the private key's neighbourhood) arrives.
  std::list<std::string> errors;
  ASSERT_TRUE(nsclient::windows_acl::protect_directory(dir_.string(), errors)) << (errors.empty() ? "" : errors.front());
  protected_ = true;

  const boost::filesystem::path outside = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-acl-src-%%%%-%%%%");
  boost::filesystem::create_directories(outside);
  const boost::filesystem::path secret_source = outside / "secret.txt";
  std::ofstream(secret_source.string().c_str()) << "password";
  // An explicit grant to BUILTIN\Users (S-1-5-32-545, by SID so any locale
  // works), so the "before" state is deterministic whatever profile the test's
  // temp directory happens to inherit from.
  const std::string grant = "icacls \"" + secret_source.string() + "\" /grant *S-1-5-32-545:R /q > nul 2>&1";
  ASSERT_EQ(std::system(grant.c_str()), 0);

  const boost::filesystem::path moved = dir_ / "moved";
  boost::system::error_code ec;
  boost::filesystem::rename(outside, moved, ec);
  ASSERT_FALSE(ec) << ec.message();
  const boost::filesystem::path secret = moved / "secret.txt";

  // The rename must have carried the grant along, or the reset below would be
  // asserting nothing.
  errors.clear();
  ASSERT_FALSE(nsclient::windows_acl::is_protected(secret.string(), errors)) << "the renamed file lost its old ACEs on its own";

  errors.clear();
  ASSERT_TRUE(nsclient::windows_acl::reset_to_inherited(moved.string(), errors)) << (errors.empty() ? "" : errors.front());
  errors.clear();
  EXPECT_TRUE(nsclient::windows_acl::is_protected(moved.string(), errors)) << (errors.empty() ? "" : errors.front());
  errors.clear();
  EXPECT_TRUE(nsclient::windows_acl::is_protected(secret.string(), errors))
      << "the file inside the renamed directory still carries its old access: " << (errors.empty() ? "" : errors.front());
}

TEST_F(AclTest, ReportsRatherThanThrowsForAMissingDirectory) {
  std::list<std::string> errors;
  const std::string missing = (dir_ / "no-such-folder").string();
  EXPECT_FALSE(nsclient::windows_acl::protect_directory(missing, errors));
  EXPECT_FALSE(errors.empty());
  errors.clear();
  EXPECT_FALSE(nsclient::windows_acl::is_protected(missing, errors));
  EXPECT_FALSE(errors.empty());
}

#endif
