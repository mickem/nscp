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
  // A library stub carrying a real PE or ELF header for `machine`.
  fs::path fxr_with_arch(const std::string &version, const std::string &bytes) const {
    const fs::path dir = fxr(version, false);
    std::ofstream((dir / dotnet::hostfxr_library_name()).string().c_str(), std::ios::binary) << bytes;
    return dir / dotnet::hostfxr_library_name();
  }
};

std::string pe_header(unsigned machine) {
  std::string s(0x40 + 6, '\0');
  s[0] = 'M';
  s[1] = 'Z';
  s[0x3c] = 0x40;  // e_lfanew
  s[0x40] = 'P';
  s[0x41] = 'E';
  s[0x44] = static_cast<char>(machine & 0xff);
  s[0x45] = static_cast<char>(machine >> 8);
  return s;
}

std::string elf_header(unsigned machine) {
  std::string s(64, '\0');
  s[0] = 0x7f;
  s[1] = 'E';
  s[2] = 'L';
  s[3] = 'F';
  s[5] = 1;  // little endian
  s[18] = static_cast<char>(machine & 0xff);
  s[19] = static_cast<char>(machine >> 8);
  return s;
}

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

TEST(dotnet_architecture, reads_pe_and_elf_machine_types) {
  temp_dir root;
  EXPECT_EQ(dotnet::architecture::x64, dotnet::library_architecture(root.fxr_with_arch("1", pe_header(0x8664))));
  EXPECT_EQ(dotnet::architecture::x86, dotnet::library_architecture(root.fxr_with_arch("2", pe_header(0x014c))));
  EXPECT_EQ(dotnet::architecture::arm64, dotnet::library_architecture(root.fxr_with_arch("3", pe_header(0xaa64))));
  EXPECT_EQ(dotnet::architecture::x64, dotnet::library_architecture(root.fxr_with_arch("4", elf_header(62))));
  EXPECT_EQ(dotnet::architecture::x86, dotnet::library_architecture(root.fxr_with_arch("5", elf_header(3))));
  EXPECT_EQ(dotnet::architecture::arm64, dotnet::library_architecture(root.fxr_with_arch("6", elf_header(183))));
  EXPECT_EQ(dotnet::architecture::unknown, dotnet::library_architecture(root.fxr_with_arch("7", "stub")));
  EXPECT_EQ(dotnet::architecture::unknown, dotnet::library_architecture(root.path / "missing.dll"));
  EXPECT_NE(dotnet::architecture::unknown, dotnet::process_architecture());
  EXPECT_STRNE("unknown", dotnet::architecture_name(dotnet::process_architecture()));
}

TEST(dotnet_locator, skips_runtimes_built_for_another_architecture) {
  // The scenario behind LoadLibrary error 193: a 32-bit agent finding the x64
  // install first. Whatever we run as, the "other" architecture must lose to
  // an older matching one, and unknown (stub) headers are still accepted.
  const dotnet::architecture mine = dotnet::process_architecture();
  const unsigned other_pe = mine == dotnet::architecture::x86 ? 0x8664 : 0x014c;
  const unsigned other_elf = mine == dotnet::architecture::x86 ? 62 : 3;
  const unsigned my_elf = mine == dotnet::architecture::x64 ? 62 : mine == dotnet::architecture::arm64 ? 183 : mine == dotnet::architecture::arm ? 40 : 3;
  const unsigned my_pe = mine == dotnet::architecture::x64     ? 0x8664
                         : mine == dotnet::architecture::arm64 ? 0xaa64
                         : mine == dotnet::architecture::arm   ? 0x01c4
                                                               : 0x014c;
  temp_dir root;
  root.fxr_with_arch("10.0.11", other_pe == 0x8664 || other_pe == 0x014c ? pe_header(other_pe) : elf_header(other_elf));
  root.fxr_with_arch("9.0.5", elf_header(other_elf));
  const fs::path good = root.fxr_with_arch("8.0.30", pe_header(my_pe));
  EXPECT_EQ(good, dotnet::find_hostfxr_in_root(root.path));

  temp_dir elf_root;
  elf_root.fxr_with_arch("10.0.0", elf_header(other_elf));
  const fs::path good_elf = elf_root.fxr_with_arch("8.0.0", elf_header(my_elf));
  EXPECT_EQ(good_elf, dotnet::find_hostfxr_in_root(elf_root.path));

  temp_dir only_other;
  only_other.fxr_with_arch("8.0.30", pe_header(other_pe));
  EXPECT_TRUE(dotnet::find_hostfxr_in_root(only_other.path).empty());
  EXPECT_FALSE(dotnet::find_hostfxr({only_other.path}).found());
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
  // The architecture-specific variable (DOTNET_ROOT_X64, ...) beats the generic one.
  std::string arch_var = std::string("DOTNET_ROOT_") + dotnet::architecture_name(dotnet::process_architecture());
  for (char &c : arch_var) c = static_cast<char>(toupper(c));
  setenv("DOTNET_ROOT", "/tmp/nscp-dotnet-root-test", 1);
  setenv(arch_var.c_str(), "/tmp/nscp-dotnet-arch-root-test", 1);
  const std::vector<fs::path> arch_roots = dotnet::default_roots("");
  unsetenv("DOTNET_ROOT");
  unsetenv(arch_var.c_str());
  ASSERT_GT(arch_roots.size(), 1u);
  EXPECT_EQ(fs::path("/tmp/nscp-dotnet-arch-root-test"), arch_roots[0]);
  EXPECT_EQ(fs::path("/tmp/nscp-dotnet-root-test"), arch_roots[1]);
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
#ifdef _WIN32
  const std::string elsewhere = "C:/elsewhere/Other";
#else
  const std::string elsewhere = "/elsewhere/Other";
#endif
  EXPECT_EQ(fs::path(elsewhere + ".dll"), DotnetPlugins::resolve_assembly(root, "alias", elsewhere + ".dll"));
  EXPECT_EQ(fs::path(elsewhere + ".dll"), DotnetPlugins::resolve_assembly(root, "alias", elsewhere));
}
