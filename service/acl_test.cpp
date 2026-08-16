// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#ifdef WIN32

#include <boost/filesystem.hpp>
#include <fstream>
#include <list>
#include <string>
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
  void restore() {
    const std::string command = "icacls \"" + dir_.string() + "\" /reset /t /q > nul 2>&1";
    const int ignored_result = std::system(command.c_str());
    static_cast<void>(ignored_result);
  }

  bool protected_ = false;
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
