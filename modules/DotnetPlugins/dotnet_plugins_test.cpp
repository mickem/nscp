// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The module's own rules for turning a configured plugin value into the
// assembly to load. Plain C++: no runtime, no managed code, no file system.
// Locating and starting the runtime is tested in libs/dotnet_host.

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "DotnetPlugins.h"

// Unit-test binaries have no generated module glue, so define the plugin
// singleton the log macros in DotnetPlugins.cpp refer to.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace fs = boost::filesystem;

TEST(dotnet_plugins, resolves_configured_plugin_values_to_assemblies) {
  const fs::path root("/opt/nsclient/modules/dotnet");
  const auto nothing = [](const fs::path &) { return false; };
  // Nothing on disk: the .dll form is what gets loaded (and named in the error).
  EXPECT_EQ(root / "MyPlugin.dll", DotnetPlugins::resolve_assembly(root, "MyPlugin", "enabled", nothing));
  EXPECT_EQ(root / "MyPlugin.dll", DotnetPlugins::resolve_assembly(root, "MyPlugin", "", nothing));
  EXPECT_EQ(root / "MyPlugin.dll", DotnetPlugins::resolve_assembly(root, "MyPlugin", "true", nothing));
  EXPECT_EQ(root / "Other.dll", DotnetPlugins::resolve_assembly(root, "alias", "Other", nothing));
  EXPECT_EQ(root / "Other.dll", DotnetPlugins::resolve_assembly(root, "alias", "Other.dll", nothing));
  EXPECT_EQ(root / "Other.DLL", DotnetPlugins::resolve_assembly(root, "alias", "Other.DLL", nothing));
  EXPECT_EQ(root / "NSCP.Plugin.Sample.dll", DotnetPlugins::resolve_assembly(root, "NSCP.Plugin.Sample", "enabled", nothing));
  EXPECT_EQ(root / "My.Plugin.dll", DotnetPlugins::resolve_assembly(root, "alias", "My.Plugin", nothing));
  EXPECT_EQ(root / "sub" / "Other.dll", DotnetPlugins::resolve_assembly(root, "alias", "sub/Other.dll", nothing));
  // A name that already says which assembly file it means is reported as such.
  EXPECT_EQ(root / "Tool.exe", DotnetPlugins::resolve_assembly(root, "alias", "Tool.exe", nothing));
#ifdef _WIN32
  const std::string elsewhere = "C:/elsewhere/Other";
#else
  const std::string elsewhere = "/elsewhere/Other";
#endif
  EXPECT_EQ(fs::path(elsewhere + ".dll"), DotnetPlugins::resolve_assembly(root, "alias", elsewhere + ".dll", nothing));
  EXPECT_EQ(fs::path(elsewhere + ".dll"), DotnetPlugins::resolve_assembly(root, "alias", elsewhere, nothing));
}

TEST(dotnet_plugins, the_configured_file_wins_when_it_exists) {
  // The value as configured is tried before ".dll" is appended, so an .exe
  // assembly or an extensionless file load as they did before.
  const fs::path root("/opt/nsclient/modules/dotnet");
  const auto only = [](const fs::path &present) { return [present](const fs::path &p) { return p == present; }; };
  EXPECT_EQ(root / "Contoso.Inventory.exe", DotnetPlugins::resolve_assembly(root, "inventory", "Contoso.Inventory.exe", only(root / "Contoso.Inventory.exe")));
  EXPECT_EQ(root / "inventory", DotnetPlugins::resolve_assembly(root, "inventory", "enabled", only(root / "inventory")));
  EXPECT_EQ(root / "Other.dll", DotnetPlugins::resolve_assembly(root, "alias", "Other", only(root / "Other.dll")));
  // With both present the configured spelling is the one that loads.
  const auto both = [root](const fs::path &p) { return p == root / "Other" || p == root / "Other.dll"; };
  EXPECT_EQ(root / "Other", DotnetPlugins::resolve_assembly(root, "alias", "Other", both));
}

TEST(dotnet_plugins, disabled_values_are_not_file_names) {
  EXPECT_TRUE(DotnetPlugins::is_disabled("disabled"));
  EXPECT_TRUE(DotnetPlugins::is_disabled("Disabled"));
  EXPECT_TRUE(DotnetPlugins::is_disabled("0"));
  EXPECT_TRUE(DotnetPlugins::is_disabled("false"));
  EXPECT_TRUE(DotnetPlugins::is_disabled("off"));
  EXPECT_FALSE(DotnetPlugins::is_disabled("enabled"));
  EXPECT_FALSE(DotnetPlugins::is_disabled(""));
  EXPECT_FALSE(DotnetPlugins::is_disabled("Disabler.dll"));
}
