// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <nscp/script_roots.hpp>
#include <string>

namespace {

namespace fs = boost::filesystem;

class ScriptRootsTest : public ::testing::Test {
 protected:
  fs::path base_;

  void SetUp() override {
    base_ = fs::temp_directory_path() / fs::unique_path("nscp-roots-%%%%-%%%%");
    fs::create_directories(base_ / "scripts" / "python");
    fs::create_directories(base_ / "elsewhere");
  }
  void TearDown() override {
    boost::system::error_code ignored;
    fs::remove_all(base_, ignored);
  }

  void write(const fs::path &p) {
    fs::create_directories(p.parent_path());
    std::ofstream(p.string().c_str()) << "x\n";
  }
};

}  // namespace

TEST_F(ScriptRootsTest, NothingIsAllowedUntilARootIsAdded) {
  // An empty list means "not configured yet", not "allow everything": a caller
  // that forgets to add its own script folder must fail closed.
  const nscp::scripts::allowed_roots roots;
  EXPECT_TRUE(roots.empty());
  EXPECT_FALSE(roots.allows(base_ / "scripts" / "any.py"));
}

TEST_F(ScriptRootsTest, AllowsAScriptInsideARootAndBelowIt) {
  nscp::scripts::allowed_roots roots;
  roots.add((base_ / "scripts").string());

  EXPECT_TRUE(roots.allows(base_ / "scripts" / "a.py"));
  EXPECT_TRUE(roots.allows(base_ / "scripts" / "python" / "b.py"));
  EXPECT_TRUE(roots.allows(base_ / "scripts" / "python" / "deep" / "c.py"));
}

TEST_F(ScriptRootsTest, RefusesAScriptOutsideEveryRoot) {
  nscp::scripts::allowed_roots roots;
  roots.add((base_ / "scripts").string());

  EXPECT_FALSE(roots.allows(base_ / "elsewhere" / "a.py"));
  EXPECT_FALSE(roots.allows(base_ / "a.py"));
}

TEST_F(ScriptRootsTest, RefusesAPathThatClimbsBackOutOfARoot) {
  // The hole this exists to close: a configured `../a.py` resolves to
  // ${scripts}/../a.py, which is the installation directory.
  nscp::scripts::allowed_roots roots;
  roots.add((base_ / "scripts").string());
  write(base_ / "a.py");

  EXPECT_FALSE(roots.allows(base_ / "scripts" / ".." / "a.py"));
}

TEST_F(ScriptRootsTest, RefusesASymlinkPointingOutOfARoot) {
#ifndef WIN32
  // Containment is lexical, so the link has to be resolved before it is
  // matched or a link planted inside an allowed folder smuggles the target in.
  nscp::scripts::allowed_roots roots;
  roots.add((base_ / "scripts").string());
  write(base_ / "elsewhere" / "target.py");

  boost::system::error_code ec;
  fs::create_symlink(base_ / "elsewhere" / "target.py", base_ / "scripts" / "link.py", ec);
  if (ec) GTEST_SKIP() << "cannot create symlinks here: " << ec.message();

  EXPECT_FALSE(roots.allows(base_ / "scripts" / "link.py"));
#endif
}

TEST_F(ScriptRootsTest, AllowsAScriptUnderAnAdditionalRoot) {
  // The reason the list is extensible: a third-party plugin package installs
  // its scripts under its own libexec, nowhere near the agent.
  nscp::scripts::allowed_roots roots;
  roots.add((base_ / "scripts").string());
  roots.add((base_ / "elsewhere").string());

  EXPECT_TRUE(roots.allows(base_ / "elsewhere" / "a.py"));
}

TEST_F(ScriptRootsTest, ReadsACommaSeparatedListAsAnOperatorWritesIt) {
  nscp::scripts::allowed_roots roots;
  roots.add_list((base_ / "scripts").string() + " , " + (base_ / "elsewhere").string());

  EXPECT_TRUE(roots.allows(base_ / "scripts" / "a.py"));
  EXPECT_TRUE(roots.allows(base_ / "elsewhere" / "b.py"));
  EXPECT_FALSE(roots.allows(base_ / "a.py"));
}

TEST_F(ScriptRootsTest, IgnoresEmptyEntries) {
  // An unset setting is a blank string, and a list an operator edited may have
  // a trailing comma; neither should add a root (least of all an empty one,
  // which would match everything).
  nscp::scripts::allowed_roots roots;
  roots.add("");
  EXPECT_TRUE(roots.empty());

  roots.add_list((base_ / "scripts").string() + ",");
  EXPECT_FALSE(roots.allows(base_ / "a.py"));
  EXPECT_TRUE(roots.allows(base_ / "scripts" / "a.py"));
}

TEST_F(ScriptRootsTest, DescribesTheRootsForTheErrorMessage) {
  nscp::scripts::allowed_roots roots;
  roots.add((base_ / "scripts").string());
  roots.add((base_ / "elsewhere").string());

  const std::string described = roots.describe();
  EXPECT_NE(described.find("scripts"), std::string::npos);
  EXPECT_NE(described.find("elsewhere"), std::string::npos);
}
