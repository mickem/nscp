// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Where the Lua ext-scr CLI looks for, and writes, scripts.
//
// The provider used to be rooted at ${base-path} with "scripts" appended back
// on in every join. That is the install base plus a literal segment rather than
// the token that already names the scripts folder, and the two are only the
// same thing on Windows - ${scripts} is ${exe-path}/scripts there, but on Linux
// ${base-path} is the directory holding the binary (/usr/sbin) while ${scripts}
// lives under the package directory. So `nscp lua list/show/add` searched, and
// imported into, a folder that does not exist on Linux.
//
// The root is now ${scripts} and the joins name what they mean. These tests pin
// the folders that are actually searched, so the segment cannot creep back in.

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>

#include "script_provider.hpp"

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace {

namespace fs = boost::filesystem;

// A core wrapper with no endpoints: find_file only calls back on failure, to
// log, and a null log endpoint is a no-op.
nscapi::core_api::FUNPTR null_loader(const char *) { return nullptr; }

nscapi::core_wrapper *test_core() {
  static nscapi::core_wrapper *core = [] {
    auto *c = new nscapi::core_wrapper();
    c->load_endpoints(&null_loader);
    return c;
  }();
  return core;
}

class LuaScriptProviderTest : public ::testing::Test {
 protected:
  fs::path root_;

  void SetUp() override {
    root_ = fs::temp_directory_path() / fs::unique_path("nscp-lua-provider-%%%%-%%%%");
    fs::create_directories(root_);
  }
  void TearDown() override {
    boost::system::error_code ignored;
    fs::remove_all(root_, ignored);
  }

  void write(const fs::path &p) {
    fs::create_directories(p.parent_path());
    std::ofstream(p.string().c_str()) << "-- test\n";
  }

  std::shared_ptr<script_provider> provider() { return std::make_shared<script_provider>(1, test_core(), root_); }
};

}  // namespace

TEST_F(LuaScriptProviderTest, RootIsTheLuaFolderUnderScriptsNotUnderAnExtraScriptsSegment) {
  EXPECT_EQ(provider()->get_root(), root_ / "lua");
  EXPECT_NE(provider()->get_root(), root_ / "scripts" / "lua") << "the ${base-path}-era 'scripts' segment came back";
}

TEST_F(LuaScriptProviderTest, FindsAScriptInTheLuaFolder) {
  write(root_ / "lua" / "mycheck.lua");
  const auto found = provider()->find_file("mycheck.lua");
  ASSERT_TRUE(found);
  EXPECT_EQ(found.value(), root_ / "lua" / "mycheck.lua");
}

TEST_F(LuaScriptProviderTest, FindsAScriptByNameWithoutTheExtension) {
  write(root_ / "lua" / "mycheck.lua");
  const auto found = provider()->find_file("mycheck");
  ASSERT_TRUE(found);
  EXPECT_EQ(found.value(), root_ / "lua" / "mycheck.lua");
}

TEST_F(LuaScriptProviderTest, FindsAScriptSittingDirectlyInTheScriptsFolder) {
  write(root_ / "loose.lua");
  const auto found = provider()->find_file("loose.lua");
  ASSERT_TRUE(found);
  EXPECT_EQ(found.value(), root_ / "loose.lua");
}

TEST_F(LuaScriptProviderTest, DoesNotLookUnderADoubledScriptsFolder) {
  // The regression this file exists for: with the old joins a script had to be
  // at <root>/scripts/lua to be found, which on a real install meant
  // ${scripts}/scripts/lua - nowhere anything puts one.
  write(root_ / "scripts" / "lua" / "doubled.lua");
  EXPECT_FALSE(provider()->find_file("doubled.lua"));
}

TEST_F(LuaScriptProviderTest, ReportsNothingForAScriptThatIsNotThere) { EXPECT_FALSE(provider()->find_file("nscp-no-such-script-here.lua")); }
