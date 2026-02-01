/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include "dll_plugin.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>

#include "plugin_interface.hpp"

using ::testing::MatchesRegex;

#ifdef WIN32
#define A_PLUGIN "modules/CheckHelpers.dll"
#else
#define A_PLUGIN "modules/libCheckHelpers.so"
#endif

// ============================================================================
// dll_plugin construction tests
// Note: These tests verify behavior when loading fails, since we can't
// easily create valid test DLLs in unit tests.
// ============================================================================

TEST(DllPluginTest, ConstructorWithNonExistentFileThrows) {
  const boost::filesystem::path nonexistent_file("nonexistent_plugin.dll");
  EXPECT_THROW({ nsclient::core::dll_plugin plugin(1, nonexistent_file, "test_alias"); }, nsclient::core::plugin_exception);
}

TEST(DllPluginTest, ConstructorWithInvalidFileThrows) {
  // Create a temporary file that is not a valid DLL
  boost::filesystem::path temp_file = boost::filesystem::temp_directory_path() / "invalid_plugin_test.dll";

  // Create an empty file
  std::ofstream ofs(temp_file.string());
  ofs << "not a valid dll";
  ofs.close();

  EXPECT_THROW({ nsclient::core::dll_plugin plugin(2, temp_file, ""); }, nsclient::core::plugin_exception);

  // Cleanup
  boost::filesystem::remove(temp_file);
}

TEST(DllPluginTest, ConstructorWithEmptyAliasAllowed) {
  const boost::filesystem::path nonexistent("test.dll");
  // This should throw due to file not existing, not due to empty alias
  EXPECT_THROW({ nsclient::core::dll_plugin plugin(3, nonexistent, ""); }, nsclient::core::plugin_exception);
}

TEST(DllPluginExceptionTest, PluginExceptionContainsModuleInfo) {
  try {
    throw nsclient::core::plugin_exception("TestModule", "Test error");
  } catch (const nsclient::core::plugin_exception& ex) {
    EXPECT_EQ(ex.file(), "TestModule");
    EXPECT_EQ(ex.reason(), "Test error");
  }
}

TEST(DllPluginExceptionTest, PluginExceptionWithPathInfo) {
  const boost::filesystem::path path("C:/plugins/test.dll");
  const std::string error_msg = "Could not load " + path.string();

  try {
    throw nsclient::core::plugin_exception(path.string(), error_msg);
  } catch (const nsclient::core::plugin_exception& ex) {
    EXPECT_EQ(ex.file(), path.string());
    EXPECT_EQ(ex.reason(), error_msg);
  }
}

// ============================================================================
// Tests for is_duplicate static behavior
// Note: This tests the expected behavior of is_duplicate method
// ============================================================================

TEST(DllPluginDuplicateTest, DifferentPathsAreNotDuplicates) {
  nsclient::core::dll_plugin plugin1(1, A_PLUGIN, "test_alias");
  EXPECT_FALSE(plugin1.is_duplicate("not-same", "test_alias"));

  nsclient::core::dll_plugin plugin2(1, A_PLUGIN, "");
  EXPECT_FALSE(plugin2.is_duplicate("not-same", ""));
}

TEST(DllPluginDuplicateTest, SamePathDifferentAliasNotDuplicate) {
  nsclient::core::dll_plugin plugin1(1, A_PLUGIN, "test_alias");
  EXPECT_FALSE(plugin1.is_duplicate(A_PLUGIN, ""));

  nsclient::core::dll_plugin plugin2(1, A_PLUGIN, "");
  EXPECT_FALSE(plugin2.is_duplicate("another_path", "test_alias"));
}

TEST(DllPluginDuplicateTest, SamePathSameAliasIsDuplicate) {
  nsclient::core::dll_plugin plugin1(1, A_PLUGIN, "test_alias");
  EXPECT_TRUE(plugin1.is_duplicate(A_PLUGIN, "test_alias"));

  nsclient::core::dll_plugin plugin2(1, A_PLUGIN, "");
  EXPECT_TRUE(plugin2.is_duplicate(A_PLUGIN, ""));
}

// ============================================================================
// Tests for module name extraction
// ============================================================================

TEST(DllPluginModuleNameTest, ModuleNameFromPath) {
  nsclient::core::dll_plugin plugin(1, A_PLUGIN, "");
  EXPECT_EQ(plugin.getName(), "CheckHelpers");
  EXPECT_FALSE(plugin.getDescription().empty());
  EXPECT_EQ(plugin.get_alias(), "");
  EXPECT_EQ(plugin.get_alias_or_name(), "CheckHelpers");
}
TEST(DllPluginModuleNameTest, ModuleNameFromAlias) {
  nsclient::core::dll_plugin plugin(1, A_PLUGIN, "test_alias");
  EXPECT_EQ(plugin.getName(), "CheckHelpers");
  EXPECT_EQ(plugin.get_alias(), "test_alias");
  EXPECT_EQ(plugin.get_alias_or_name(), "test_alias");
}

// ============================================================================
// Tests for version string formatting
// ============================================================================

TEST(DllPluginVersionTest, VersionStringFormat) {
  nsclient::core::dll_plugin plugin(1, A_PLUGIN, "test_alias");

  EXPECT_THAT(plugin.get_version(), MatchesRegex(R"(\d+\.\d+\.\d+)"));
}
