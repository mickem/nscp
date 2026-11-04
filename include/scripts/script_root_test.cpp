// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <map>
#include <scripts/script_root.hpp>
#include <string>

namespace fs = boost::filesystem;

namespace {

// A stand-in for the core's expand_path: the tokens the resolver may hand it,
// and nothing else - an unknown token is an error there, so it is one here.
std::string expand(const std::string &value) {
  static const std::map<std::string, std::string> tokens = {
      {"${scripts}", "/opt/nscp/scripts"},
      {"${fleet-folder}", "/var/lib/nsclient/fleet"},
      {"${shared-path}", "/usr/lib/nsclient"},
      {"${data-path}", "/var/lib/nsclient"},
  };
  std::string out = value;
  for (const auto &t : tokens) {
    const std::string::size_type at = out.find(t.first);
    if (at != std::string::npos) out.replace(at, t.first.size(), t.second);
  }
  if (out.find("${") != std::string::npos) throw std::runtime_error("Unknown path token in " + value);
  return out;
}

}  // namespace

// The built-in layout: the scripts folder is ${scripts}, whatever it is called,
// and scripts run in the folder above it - the installation directory on a
// default install.
TEST(ScriptRoot, EmptyIsTheBuiltInLayout) {
  const scripts::root::layout l = scripts::root::resolve("", expand);
  EXPECT_FALSE(l.configured);
  EXPECT_EQ(l.scripts.generic_string(), "/opt/nscp/scripts");
  EXPECT_EQ(l.base.generic_string(), "/opt/nscp");
}

TEST(ScriptRoot, DefaultSpelledOutIsTheBuiltInLayoutToo) {
  for (const char *spelling : {"default", "Default", " DEFAULT ", "   "}) {
    const scripts::root::layout l = scripts::root::resolve(spelling, expand);
    EXPECT_FALSE(l.configured) << spelling;
    EXPECT_EQ(l.scripts.generic_string(), "/opt/nscp/scripts") << spelling;
  }
}

// The key always named the scripts folder and defaulted to ${scripts}; the
// docs told operators to write that line out. Written out - as the token, or
// as the folder it expands to, however spelled - it is the default, not a
// configured root: on Windows the built-in layout keeps the installation
// directory as the start directory under a `[paths] scripts` override, and
// that must hold for the value every pre-existing nsclient.ini was told to
// carry.
TEST(ScriptRoot, TheDefaultWrittenOutIsNotConfigured) {
  for (const char *spelling : {"${scripts}", " ${scripts} ", "/opt/nscp/scripts", "/opt/nscp/scripts/", "/opt/nscp/./scripts"}) {
    const scripts::root::layout l = scripts::root::resolve(spelling, expand);
    EXPECT_FALSE(l.configured) << spelling;
    EXPECT_EQ(l.scripts.generic_string(), "/opt/nscp/scripts") << spelling;
    EXPECT_EQ(l.base.generic_string(), "/opt/nscp") << spelling;
  }
  EXPECT_TRUE(scripts::root::resolve("fleet", expand).configured);
  EXPECT_TRUE(scripts::root::resolve("/opt/nscp/other", expand).configured);
}

// `fleet` is the tree the sync stages bundles into: the scripts folder is the
// fleet folder's `scripts`, which is where a bundle's `scripts/` entries are
// written, and scripts run in the fleet folder - so `scripts\a.bat` is the
// same spelling it has in the installation directory.
TEST(ScriptRoot, FleetIsTheFleetFoldersScripts) {
  const scripts::root::layout l = scripts::root::resolve("fleet", expand);
  EXPECT_TRUE(l.configured);
  EXPECT_EQ(l.scripts.generic_string(), "/var/lib/nsclient/fleet/scripts");
  EXPECT_EQ(l.base.generic_string(), "/var/lib/nsclient/fleet");
}

TEST(ScriptRoot, AliasesAreCaseInsensitive) {
  EXPECT_EQ(scripts::root::resolve("FLEET", expand).scripts.generic_string(), "/var/lib/nsclient/fleet/scripts");
  EXPECT_EQ(scripts::root::resolve("Shared", expand).scripts.generic_string(), "/usr/lib/nsclient/scripts");
}

TEST(ScriptRoot, SharedIsTheSharedPathsScripts) {
  const scripts::root::layout l = scripts::root::resolve("shared", expand);
  EXPECT_EQ(l.scripts.generic_string(), "/usr/lib/nsclient/scripts");
  EXPECT_EQ(l.base.generic_string(), "/usr/lib/nsclient");
}

// Anything else is the scripts folder itself, tokens expanded; it need not be
// called `scripts`, and its parent is still where scripts run.
TEST(ScriptRoot, APathIsTheScriptsFolderAndItsParentTheBase) {
  const scripts::root::layout l = scripts::root::resolve("${data-path}/mine", expand);
  EXPECT_TRUE(l.configured);
  EXPECT_EQ(l.scripts.generic_string(), "/var/lib/nsclient/mine");
  EXPECT_EQ(l.base.generic_string(), "/var/lib/nsclient");
}

// A trailing separator must not make the folder its own parent.
TEST(ScriptRoot, ATrailingSeparatorIsDropped) {
  EXPECT_EQ(scripts::root::resolve("/srv/nscp/scripts/", expand).scripts.generic_string(), "/srv/nscp/scripts");
  EXPECT_EQ(scripts::root::resolve("/srv/nscp/scripts/", expand).base.generic_string(), "/srv/nscp");
  EXPECT_EQ(scripts::root::resolve("/srv/nscp/scripts\\", expand).scripts.generic_string(), "/srv/nscp/scripts");
}

// A root directory keeps its separator: stripped, `C:\` becomes the
// drive-relative `C:`.
TEST(ScriptRoot, ARootDirectoryKeepsItsSeparator) {
#ifdef WIN32
  EXPECT_EQ(scripts::root::without_trailing_separator("C:\\").string(), "C:\\");
  EXPECT_EQ(scripts::root::without_trailing_separator("C:\\nscp\\").string(), "C:\\nscp");
#else
  EXPECT_EQ(scripts::root::without_trailing_separator("/").string(), "/");
  EXPECT_EQ(scripts::root::without_trailing_separator("//server/").string(), "//server/");
  EXPECT_EQ(scripts::root::without_trailing_separator("//server/share/").string(), "//server/share");
#endif
}

// A scripts folder with no parent has no base; the module falls back to the
// installation directory rather than an empty working directory.
TEST(ScriptRoot, ABareNameHasNoBase) { EXPECT_TRUE(scripts::root::resolve("scripts", expand).base.empty()); }

// A scripts folder that is a root directory has no folder above it either.
// boost spells the parent of `C:\` as the drive-relative `C:`, which as a
// working directory or a rooting base would resolve against the per-drive
// current directory; that must come out as "no base", not as `C:`.
TEST(ScriptRoot, ARootDirectoryHasNoBase) {
#ifdef WIN32
  const scripts::root::layout l = scripts::root::resolve("C:\\", expand);
  EXPECT_EQ(l.scripts.string(), "C:\\");
  EXPECT_TRUE(l.base.empty()) << l.base.string();
  EXPECT_TRUE(scripts::root::parent_folder("C:").empty());
  EXPECT_EQ(scripts::root::parent_folder("C:\\nscp\\scripts").string(), "C:\\nscp");
#else
  const scripts::root::layout l = scripts::root::resolve("/", expand);
  EXPECT_EQ(l.scripts.string(), "/");
  EXPECT_TRUE(l.base.empty()) << l.base.string();
  EXPECT_EQ(scripts::root::parent_folder("/opt/nscp/scripts").string(), "/opt/nscp");
#endif
}

// The folder a module publishes to its query threads: empty until set, and
// then the last value set.
TEST(ScriptRoot, APublishedFolderIsEmptyUntilSet) {
  scripts::root::published_folder folder;
  EXPECT_TRUE(folder.get().empty());
  folder.set("/opt/nscp/scripts");
  EXPECT_EQ(folder.get().generic_string(), "/opt/nscp/scripts");
  folder.set("/var/lib/nsclient/fleet/scripts");
  EXPECT_EQ(folder.get().generic_string(), "/var/lib/nsclient/fleet/scripts");
}

// An unknown token is the operator's typo, and is reported the way every
// other path option reports it rather than silently becoming a folder name.
TEST(ScriptRoot, AnUnknownTokenIsReported) { EXPECT_THROW(scripts::root::resolve("${scripst}/x", expand), std::runtime_error); }

// Whatever an alias stands for is spelled as the token, so the core's own
// expansion (and its precedence rules for [paths] overrides) decides where it
// is; the resolver never hard-codes a location.
TEST(ScriptRoot, AliasTargetsAreTokens) {
  EXPECT_EQ(scripts::root::alias_target("fleet"), "${fleet-folder}/scripts");
  EXPECT_EQ(scripts::root::alias_target("shared"), "${shared-path}/scripts");
  EXPECT_EQ(scripts::root::alias_target("/x"), "/x");
}
