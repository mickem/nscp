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
