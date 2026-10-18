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
      {"${scripts}", "/opt/nscp/custom-scripts"},
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

// The built-in layout keeps ${scripts} as it is, whatever it is called: an
// override can point it at a folder not named `scripts`, and deriving it from
// a parent would move it.
TEST(ScriptRoot, EmptyIsTheBuiltInLayout) {
  const scripts::root::layout l = scripts::root::resolve("", expand);
  EXPECT_FALSE(l.custom());
  EXPECT_TRUE(l.root.empty());
  EXPECT_EQ(l.scripts.generic_string(), "/opt/nscp/custom-scripts");
}

TEST(ScriptRoot, DefaultSpelledOutIsTheBuiltInLayoutToo) {
  for (const char *spelling : {"default", "Default", " DEFAULT ", "   "}) {
    const scripts::root::layout l = scripts::root::resolve(spelling, expand);
    EXPECT_FALSE(l.custom()) << spelling;
    EXPECT_EQ(l.scripts.generic_string(), "/opt/nscp/custom-scripts") << spelling;
  }
}

// `fleet` is the tree the sync stages bundles into: the root is the fleet
// folder itself and the scripts sit in its `scripts` sub-folder, which is where
// a bundle's `scripts/` entries are written.
TEST(ScriptRoot, FleetIsTheFleetFolder) {
  const scripts::root::layout l = scripts::root::resolve("fleet", expand);
  EXPECT_TRUE(l.custom());
  EXPECT_EQ(l.root.generic_string(), "/var/lib/nsclient/fleet");
  EXPECT_EQ(l.scripts.generic_string(), "/var/lib/nsclient/fleet/scripts");
}

TEST(ScriptRoot, AliasesAreCaseInsensitive) {
  EXPECT_EQ(scripts::root::resolve("FLEET", expand).root.generic_string(), "/var/lib/nsclient/fleet");
  EXPECT_EQ(scripts::root::resolve("Shared", expand).root.generic_string(), "/usr/lib/nsclient");
}

TEST(ScriptRoot, SharedIsTheSharedPath) {
  const scripts::root::layout l = scripts::root::resolve("shared", expand);
  EXPECT_EQ(l.root.generic_string(), "/usr/lib/nsclient");
  EXPECT_EQ(l.scripts.generic_string(), "/usr/lib/nsclient/scripts");
}

// Anything else is a folder, tokens expanded, with the install layout below
// it.
TEST(ScriptRoot, APathIsExpandedAndGetsTheScriptsFolderBelowIt) {
  const scripts::root::layout l = scripts::root::resolve("${data-path}/mine", expand);
  EXPECT_TRUE(l.custom());
  EXPECT_EQ(l.root.generic_string(), "/var/lib/nsclient/mine");
  EXPECT_EQ(l.scripts.generic_string(), "/var/lib/nsclient/mine/scripts");
}

// A trailing separator must not produce `<root>//scripts` or a root that
// compares unequal to the same folder written without it.
TEST(ScriptRoot, ATrailingSeparatorIsDropped) {
  EXPECT_EQ(scripts::root::resolve("/srv/nscp/", expand).root.generic_string(), "/srv/nscp");
  EXPECT_EQ(scripts::root::resolve("/srv/nscp/", expand).scripts.generic_string(), "/srv/nscp/scripts");
  EXPECT_EQ(scripts::root::resolve("/srv/nscp\\", expand).root.generic_string(), "/srv/nscp");
}

// The old CheckExternalScripts key named the scripts folder itself and
// defaulted to ${scripts}; the docs told operators to write that line out. A
// host upgrading with it must keep the layout it had, not get a `scripts`
// folder appended, a sandbox on a folder that does not exist and, on
// Windows, a working directory that moved.
TEST(ScriptRoot, TheOldDefaultWrittenOutIsTheBuiltInLayout) {
  for (const char *spelling : {"${scripts}", " ${scripts} ", "/opt/nscp/custom-scripts", "/opt/nscp/custom-scripts/", "/opt/nscp/./custom-scripts"}) {
    const scripts::root::layout l = scripts::root::resolve(spelling, expand);
    EXPECT_FALSE(l.custom()) << spelling;
    EXPECT_TRUE(l.legacy) << spelling;
    EXPECT_EQ(l.scripts.generic_string(), "/opt/nscp/custom-scripts") << spelling;
  }
  EXPECT_FALSE(scripts::root::resolve("fleet", expand).legacy);
  EXPECT_FALSE(scripts::root::resolve("", expand).legacy);
}

// A root directory keeps its separator: stripped, `C:\` becomes the
// drive-relative `C:` and the scripts folder `C:scripts`.
TEST(ScriptRoot, ARootDirectoryKeepsItsSeparator) {
#ifdef WIN32
  EXPECT_EQ(scripts::root::without_trailing_separator("C:\\").string(), "C:\\");
  EXPECT_EQ(scripts::root::resolve("C:\\", expand).scripts.string(), "C:\\scripts");
  EXPECT_EQ(scripts::root::without_trailing_separator("C:\\nscp\\").string(), "C:\\nscp");
#else
  EXPECT_EQ(scripts::root::without_trailing_separator("/").string(), "/");
  EXPECT_EQ(scripts::root::without_trailing_separator("//server/").string(), "//server/");
  EXPECT_EQ(scripts::root::without_trailing_separator("//server/share/").string(), "//server/share");
#endif
}

// An unknown token is the operator's typo, and is reported the way every
// other path option reports it rather than silently becoming a folder name.
TEST(ScriptRoot, AnUnknownTokenIsReported) { EXPECT_THROW(scripts::root::resolve("${scripst}/x", expand), std::runtime_error); }

// Whatever an alias stands for is spelled as the token, so the core's own
// expansion (and its precedence rules for [paths] overrides) decides where it
// is; the resolver never hard-codes a location.
TEST(ScriptRoot, AliasTargetsAreTokens) {
  EXPECT_EQ(scripts::root::alias_target("fleet"), "${fleet-folder}");
  EXPECT_EQ(scripts::root::alias_target("shared"), "${shared-path}");
  EXPECT_EQ(scripts::root::alias_target("/x"), "/x");
}
