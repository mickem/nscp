// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <string>

#include "script_paths.hpp"

namespace fs = boost::filesystem;

namespace {

// A real directory tree, because relative_to canonicalises both sides and a
// purely notional path would exercise only the fallback.
class ScriptPathsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    base_ = fs::temp_directory_path() / fs::unique_path("nscp-paths-%%%%%%%%");
    fs::create_directories(base_ / "scripts");
    fs::create_directories(base_ / "custom" / "deep");
    elsewhere_ = fs::temp_directory_path() / fs::unique_path("nscp-elsewhere-%%%%%%%%");
    fs::create_directories(elsewhere_);
  }
  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(base_, ec);
    fs::remove_all(elsewhere_, ec);
  }
  fs::path base_;
  fs::path elsewhere_;
};

// The default layout: the script root sits under the install directory, so the
// short spelling is available and is what gets recorded.
TEST_F(ScriptPathsTest, AFileUnderTheBaseIsNamedRelativeToIt) {
  const std::string rel = script_paths::relative_to(base_, base_ / "scripts" / "check_x.bat");
  ASSERT_FALSE(rel.empty());
  EXPECT_EQ(fs::path(rel).generic_string(), "scripts/check_x.bat");
}

// A `script root` pointed somewhere else entirely. This is the case the old
// hard-coded "scripts\<name>" got wrong: it recorded a command naming a file
// that was never written, and the check exited 127.
TEST_F(ScriptPathsTest, AFileOutsideTheBaseIsNotNamedRelativeToIt) {
  EXPECT_EQ(script_paths::relative_to(base_, elsewhere_ / "check_x.bat"), "");
}

// `script root` moved deeper rather than away: still below the base, so still
// recordable - but as the folder it is really in, not as "scripts".
TEST_F(ScriptPathsTest, ADeeperScriptRootKeepsItsOwnFolderName) {
  const std::string rel = script_paths::relative_to(base_, base_ / "custom" / "deep" / "check_x.bat");
  ASSERT_FALSE(rel.empty());
  EXPECT_EQ(fs::path(rel).generic_string(), "custom/deep/check_x.bat");
}

// A sibling of the install directory shares its prefix as a string but is not
// below it; a plain starts_with test would accept it.
TEST_F(ScriptPathsTest, ASiblingSharingATextualPrefixIsRejected) {
  const fs::path sibling = base_.string() + "-other";
  fs::create_directories(sibling);
  EXPECT_EQ(script_paths::relative_to(base_, sibling / "check_x.bat"), "");
  boost::system::error_code ec;
  fs::remove_all(sibling, ec);
}

// Climbing out with ".." yields a value that resolves only from one working
// directory, which is the thing being avoided.
TEST_F(ScriptPathsTest, APathThatWouldClimbOutIsRejected) {
  EXPECT_EQ(script_paths::relative_to(base_ / "scripts", base_ / "other.bat"), "");
}

TEST_F(ScriptPathsTest, AnEmptyBaseNamesNothing) { EXPECT_EQ(script_paths::relative_to("", base_ / "scripts" / "check_x.bat"), ""); }

// The separators have to be the platform's own: the recorded value is handed to
// the shell verbatim, and on Windows a forward slash in a bare relative command
// is not what the historic spelling looked like.
TEST_F(ScriptPathsTest, TheResultUsesThePlatformsOwnSeparators) {
  const std::string rel = script_paths::relative_to(base_, base_ / "scripts" / "check_x.bat");
  ASSERT_FALSE(rel.empty());
#ifdef WIN32
  EXPECT_NE(rel.find('\\'), std::string::npos) << rel;
  EXPECT_EQ(rel.find('/'), std::string::npos) << rel;
#else
  EXPECT_NE(rel.find('/'), std::string::npos) << rel;
#endif
}

}  // namespace
