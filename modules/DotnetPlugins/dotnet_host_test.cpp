// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "dotnet_host.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "DotnetPlugins.h"

// Unit-test binaries have no generated module glue, so define the plugin
// singleton the log macros in DotnetPlugins.cpp refer to.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace fs = boost::filesystem;

namespace {

struct temp_dir {
  fs::path path;
  temp_dir() : path(fs::temp_directory_path() / fs::unique_path("nscp-dotnet-%%%%-%%%%")) { fs::create_directories(path); }
  ~temp_dir() {
    boost::system::error_code ec;
    fs::remove_all(path, ec);
  }
  fs::path fxr(const std::string &version, bool with_library = true) const {
    const fs::path dir = path / "host" / "fxr" / version;
    fs::create_directories(dir);
    if (with_library) std::ofstream((dir / dotnet::hostfxr_library_name()).string().c_str()) << "stub";
    return dir;
  }
};

dotnet::version v(const std::string &text) {
  dotnet::version out;
  EXPECT_TRUE(dotnet::parse_version(text, out)) << text;
  return out;
}

}  // namespace

TEST(dotnet_version, parses_release_and_prerelease) {
  dotnet::version release = v("8.0.30");
  ASSERT_EQ(3u, release.parts.size());
  EXPECT_EQ(8, release.parts[0]);
  EXPECT_EQ(0, release.parts[1]);
  EXPECT_EQ(30, release.parts[2]);
  EXPECT_TRUE(release.prerelease.empty());
  EXPECT_EQ("8.0.30", release.text);

  dotnet::version preview = v("9.0.0-preview.3.24172.9");
  ASSERT_EQ(3u, preview.parts.size());
  EXPECT_EQ(9, preview.parts[0]);
  EXPECT_EQ("preview.3.24172.9", preview.prerelease);
}

TEST(dotnet_version, rejects_folders_that_are_not_versions) {
  dotnet::version out;
  EXPECT_FALSE(dotnet::parse_version("", out));
  EXPECT_FALSE(dotnet::parse_version("latest", out));
  EXPECT_FALSE(dotnet::parse_version("8..0", out));
  EXPECT_FALSE(dotnet::parse_version("8.0.x", out));
  EXPECT_FALSE(dotnet::parse_version("-preview", out));
}

TEST(dotnet_version, orders_numerically_with_releases_above_prereleases) {
  EXPECT_LT(v("8.0.30"), v("9.0.0"));
  EXPECT_LT(v("8.0.9"), v("8.0.10"));
  EXPECT_LT(v("8.0"), v("8.0.1"));
  EXPECT_LT(v("9.0.0-rc.1"), v("9.0.0"));
  EXPECT_LT(v("9.0.0-preview.1"), v("9.0.0-rc.1"));
  EXPECT_LT(v("8.0.30"), v("9.0.0-preview.1"));
  EXPECT_FALSE(v("8.0.30") < v("8.0.30"));
}

TEST(dotnet_locator, picks_the_newest_fxr_folder_that_has_the_library) {
  temp_dir root;
  root.fxr("8.0.30");
  root.fxr("9.0.1-preview.2");
  root.fxr("10.0.0", /*with_library=*/false);
  fs::create_directories(root.path / "host" / "fxr" / "not-a-version");
  const fs::path found = dotnet::find_hostfxr_in_root(root.path);
  EXPECT_EQ(root.path / "host" / "fxr" / "9.0.1-preview.2" / dotnet::hostfxr_library_name(), found);
}

TEST(dotnet_locator, prefers_a_release_over_a_prerelease_of_the_same_number) {
  temp_dir root;
  root.fxr("9.0.0-rc.2");
  root.fxr("9.0.0");
  EXPECT_EQ(root.path / "host" / "fxr" / "9.0.0" / dotnet::hostfxr_library_name(), dotnet::find_hostfxr_in_root(root.path));
}

TEST(dotnet_locator, returns_nothing_for_roots_without_a_runtime) {
  temp_dir root;
  EXPECT_TRUE(dotnet::find_hostfxr_in_root(root.path).empty());
  EXPECT_TRUE(dotnet::find_hostfxr_in_root(root.path / "missing").empty());
  fs::create_directories(root.path / "host" / "fxr");
  EXPECT_TRUE(dotnet::find_hostfxr_in_root(root.path).empty());
}

TEST(dotnet_locator, searches_roots_in_order_and_reports_what_it_looked_at) {
  temp_dir empty;
  temp_dir first;
  temp_dir second;
  first.fxr("8.0.1");
  second.fxr("9.0.0");
  const dotnet::hostfxr_location found = dotnet::find_hostfxr({empty.path, first.path, second.path});
  ASSERT_TRUE(found.found());
  EXPECT_EQ(first.path, found.root);
  EXPECT_EQ(first.path / "host" / "fxr" / "8.0.1" / dotnet::hostfxr_library_name(), found.library);
  ASSERT_EQ(2u, found.searched.size());
  EXPECT_EQ(empty.path.string(), found.searched[0]);
  EXPECT_EQ(first.path.string(), found.searched[1]);

  const dotnet::hostfxr_location missing = dotnet::find_hostfxr({empty.path});
  EXPECT_FALSE(missing.found());
  EXPECT_EQ(1u, missing.searched.size());
}

TEST(dotnet_locator, an_explicit_root_is_searched_first_and_defaults_follow) {
  const std::vector<fs::path> roots = dotnet::default_roots("/opt/my-dotnet");
  ASSERT_FALSE(roots.empty());
  EXPECT_EQ(fs::path("/opt/my-dotnet"), roots[0]);
  // The platform defaults are always appended after the explicit choices.
  EXPECT_GT(roots.size(), 1u);
  const std::vector<fs::path> without = dotnet::default_roots("");
  EXPECT_EQ(roots.size(), without.size() + 1);
  EXPECT_EQ(without[0], roots[1]);
}

#ifndef _WIN32
TEST(dotnet_locator, honours_DOTNET_ROOT) {
  setenv("DOTNET_ROOT", "/tmp/nscp-dotnet-root-test", 1);
  const std::vector<fs::path> roots = dotnet::default_roots("");
  unsetenv("DOTNET_ROOT");
  ASSERT_FALSE(roots.empty());
  EXPECT_EQ(fs::path("/tmp/nscp-dotnet-root-test"), roots[0]);
  // A duplicate root (override == DOTNET_ROOT) is listed once.
  setenv("DOTNET_ROOT", "/tmp/nscp-dotnet-root-test", 1);
  const std::vector<fs::path> both = dotnet::default_roots("/tmp/nscp-dotnet-root-test");
  unsetenv("DOTNET_ROOT");
  EXPECT_EQ(roots.size(), both.size());
}
#endif

TEST(dotnet_host, the_runtime_host_is_a_process_wide_singleton) {
  EXPECT_EQ(dotnet::host::instance().get(), dotnet::host::instance().get());
  EXPECT_EQ("not loaded", dotnet::host::instance()->describe());
}

TEST(dotnet_host, refuses_a_missing_runtime_with_a_reason) {
  std::string error;
  EXPECT_FALSE(dotnet::host::instance()->initialize(dotnet::hostfxr_location(), "x.runtimeconfig.json", error));
  EXPECT_EQ("No .NET runtime found", error);
  void *fn = dotnet::host::instance()->get_function("NSCP.Core.dll", "T", "M", error);
  EXPECT_EQ(nullptr, fn);
  EXPECT_EQ("The .NET runtime is not initialized", error);
}

TEST(dotnet_plugins, resolves_configured_plugin_values_to_assemblies) {
  const fs::path root("/opt/nsclient/modules/dotnet");
  EXPECT_EQ(root / "MyPlugin.dll", DotnetPlugins::resolve_assembly(root, "MyPlugin", "enabled"));
  EXPECT_EQ(root / "MyPlugin.dll", DotnetPlugins::resolve_assembly(root, "MyPlugin", ""));
  EXPECT_EQ(root / "MyPlugin.dll", DotnetPlugins::resolve_assembly(root, "MyPlugin", "true"));
  EXPECT_EQ(root / "Other.dll", DotnetPlugins::resolve_assembly(root, "alias", "Other"));
  EXPECT_EQ(root / "Other.dll", DotnetPlugins::resolve_assembly(root, "alias", "Other.dll"));
  EXPECT_EQ(root / "Other.DLL", DotnetPlugins::resolve_assembly(root, "alias", "Other.DLL"));
  EXPECT_EQ(root / "NSCP.Plugin.Sample.dll", DotnetPlugins::resolve_assembly(root, "NSCP.Plugin.Sample", "enabled"));
  EXPECT_EQ(root / "My.Plugin.dll", DotnetPlugins::resolve_assembly(root, "alias", "My.Plugin"));
  EXPECT_EQ(root / "sub" / "Other.dll", DotnetPlugins::resolve_assembly(root, "alias", "sub/Other.dll"));
  EXPECT_EQ(fs::path("/elsewhere/Other.dll"), DotnetPlugins::resolve_assembly(root, "alias", "/elsewhere/Other.dll"));
  EXPECT_EQ(fs::path("/elsewhere/Other.dll"), DotnetPlugins::resolve_assembly(root, "alias", "/elsewhere/Other"));
}
