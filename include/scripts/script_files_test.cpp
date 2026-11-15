// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <scripts/script_files.hpp>
#include <string>

namespace fs = boost::filesystem;
namespace sf = scripts::files;

namespace {

void write(const fs::path &p, const std::string &data) {
  fs::create_directories(p.parent_path());
  std::ofstream out(p.string().c_str(), std::ios::out | std::ios::binary);
  out << data;
}

// ${scripts}/lua under a fresh temporary directory, plus a sibling folder
// outside the scripts tree for the escape attempts to aim at.
class ScriptFiles : public ::testing::Test {
 protected:
  fs::path base;
  fs::path scripts;
  fs::path root;
  fs::path outside;

  void SetUp() override {
    base = fs::weakly_canonical(fs::temp_directory_path() / fs::unique_path("nscp-script-files-%%%%-%%%%"));
    scripts = base / "scripts";
    root = scripts / "lua";
    outside = base / "outside";
    fs::create_directories(root);
    fs::create_directories(outside);
    write(root / "foo.lua", "print('foo')");
    write(root / "lib" / "helper.lua", "return {}");
    write(root / "sub" / "bar.lua", "print('bar')");
    write(scripts / "top.lua", "print('top')");
    write(outside / "secret.lua", "secret");
  }
  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(base, ec);
  }

  boost::optional<fs::path> resolve(const std::string &name, sf::sandbox_use use, bool &out) const {
    return sf::resolve_in_sandbox(root, name, ".lua", use, out);
  }
};

}  // namespace

// The three spellings a script is named by - `lua/foo.lua` (what `list` and
// the REST listing print), `foo.lua` and `foo` (what the configuration holds)
// - all reach the same file inside the folder.
TEST_F(ScriptFiles, ResolvesEveryAcceptedSpelling) {
  for (const char *name : {"foo", "foo.lua", "lua/foo.lua", "lua/foo"}) {
    bool out = true;
    const auto r = resolve(name, sf::sandbox_use::read, out);
    ASSERT_TRUE(r) << name;
    EXPECT_FALSE(out) << name;
    EXPECT_TRUE(sf::same_entry(r.value(), root / "foo.lua")) << name;
  }
  bool out = true;
  const auto sub = resolve("sub/bar", sf::sandbox_use::read, out);
  ASSERT_TRUE(sub);
  EXPECT_TRUE(sf::same_entry(sub.value(), root / "sub" / "bar.lua"));
}

TEST_F(ScriptFiles, MissingOrEmptyNameResolvesToNothing) {
  bool out = true;
  EXPECT_FALSE(resolve("", sf::sandbox_use::read, out));
  EXPECT_FALSE(out);
  EXPECT_FALSE(resolve("nope", sf::sandbox_use::read, out));
  EXPECT_FALSE(out);
}

// `..` must not climb out of the folder, and when the only match is outside
// the caller is told so, so it can say why it refused rather than "not found".
TEST_F(ScriptFiles, DotDotCannotLeaveTheFolder) {
  bool out = false;
  EXPECT_FALSE(resolve("../../outside/secret.lua", sf::sandbox_use::read, out));
  EXPECT_TRUE(out);
  out = false;
  EXPECT_FALSE(resolve("../../outside/secret", sf::sandbox_use::remove, out));
  EXPECT_TRUE(out);
}

// ${scripts}/top.lua is a sibling of the folder, not in it: the `parent /
// name` candidate exists for the `lua/foo.lua` spelling only.
TEST_F(ScriptFiles, SiblingOfTheFolderIsOutside) {
  bool out = false;
  EXPECT_FALSE(resolve("top.lua", sf::sandbox_use::read, out));
  EXPECT_TRUE(out);
}

TEST_F(ScriptFiles, AbsoluteNameIsNotTakenAsGiven) {
  bool out = false;
  EXPECT_FALSE(resolve((outside / "secret.lua").string(), sf::sandbox_use::read, out));
}

// A directory is never a candidate, for either use.
TEST_F(ScriptFiles, DirectoryIsNeverACandidate) {
  bool out = true;
  EXPECT_FALSE(resolve("sub", sf::sandbox_use::read, out));
  EXPECT_FALSE(resolve("sub", sf::sandbox_use::remove, out));
}

#ifndef WIN32
// A link inside the folder pointing out of it: reading follows the link, so
// it is refused; removing it only removes the link, so it is allowed - and
// the path returned is the link, never its target.
TEST_F(ScriptFiles, SymlinkOutOfTheFolder) {
  fs::create_symlink(outside / "secret.lua", root / "escape.lua");
  bool out = false;
  EXPECT_FALSE(resolve("escape", sf::sandbox_use::read, out));
  EXPECT_TRUE(out);

  out = true;
  const auto r = resolve("escape", sf::sandbox_use::remove, out);
  ASSERT_TRUE(r);
  EXPECT_FALSE(out);
  EXPECT_EQ(r.value().filename().string(), "escape.lua");
  EXPECT_TRUE(fs::is_symlink(fs::symlink_status(r.value())));
}

// A dangling link can still be removed, but not read.
TEST_F(ScriptFiles, DanglingSymlinkCanOnlyBeRemoved) {
  fs::create_symlink(base / "gone.lua", root / "dangling.lua");
  bool out = false;
  EXPECT_FALSE(resolve("dangling", sf::sandbox_use::read, out));
  out = true;
  EXPECT_TRUE(resolve("dangling", sf::sandbox_use::remove, out));
  EXPECT_FALSE(out);
}

// A symlinked sub-folder: lexically inside, really outside. Neither reading
// nor removing a file through it is allowed - removing would delete a file
// outside the folder.
TEST_F(ScriptFiles, SymlinkedSubFolderIsOutside) {
  fs::create_directory_symlink(outside, root / "linked");
  for (const sf::sandbox_use use : {sf::sandbox_use::read, sf::sandbox_use::remove}) {
    bool out = false;
    EXPECT_FALSE(resolve("linked/secret", use, out));
    EXPECT_TRUE(out);
  }
  EXPECT_TRUE(fs::exists(outside / "secret.lua"));
}

// A link to a file inside the folder reads fine.
TEST_F(ScriptFiles, SymlinkWithinTheFolderReads) {
  fs::create_symlink(root / "foo.lua", root / "alias.lua");
  bool out = true;
  const auto r = resolve("alias", sf::sandbox_use::read, out);
  ASSERT_TRUE(r);
  EXPECT_FALSE(out);
}

// same_entry compares directory entries, not targets: deleting a link must
// not also drop the configuration entry of the file it points at.
TEST_F(ScriptFiles, SameEntryDoesNotFollowTheLink) {
  fs::create_symlink(root / "foo.lua", root / "alias.lua");
  EXPECT_FALSE(sf::same_entry(root / "alias.lua", root / "foo.lua"));
  EXPECT_TRUE(sf::same_entry(root / "alias.lua", root / "sub" / ".." / "alias.lua"));
}
#endif

TEST_F(ScriptFiles, SameEntryNormalisesSpellings) {
  EXPECT_TRUE(sf::same_entry(root / "foo.lua", root / "." / "foo.lua"));
  EXPECT_TRUE(sf::same_entry(root / "foo.lua", root / "sub" / ".." / "foo.lua"));
  EXPECT_FALSE(sf::same_entry(root / "foo.lua", root / "sub" / "bar.lua"));
}

// configured_file finds the file the way the loaders do: as written, then in
// ${scripts}/<folder>, then ${scripts}, each with and without the extension,
// and with `\` read as a separator.
TEST_F(ScriptFiles, ConfiguredFileSearchOrder) {
  auto find = [this](const std::string &s) { return sf::configured_file(scripts, "lua", ".lua", {s}); };
  ASSERT_TRUE(find("foo"));
  EXPECT_EQ(find("foo").value(), scripts / "lua" / "foo.lua");
  ASSERT_TRUE(find("top"));
  EXPECT_EQ(find("top").value(), scripts / "top.lua");
  ASSERT_TRUE(find("sub\\bar"));
  EXPECT_EQ(find("sub/bar").value(), scripts / "lua" / "sub" / "bar.lua");
  ASSERT_TRUE(find((outside / "secret.lua").string()));
  EXPECT_FALSE(find("nope"));
  // The expanded spelling is tried when the raw one misses.
  const auto both = sf::configured_file(scripts, "lua", ".lua", {"${missing}/foo", "foo"});
  ASSERT_TRUE(both);
  EXPECT_EQ(both.value(), scripts / "lua" / "foo.lua");
}

TEST_F(ScriptFiles, ReadFile) {
  const auto data = sf::read_file(root / "foo.lua");
  ASSERT_TRUE(data);
  EXPECT_EQ(data.value(), "print('foo')");
  write(root / "empty.lua", "");
  const auto empty = sf::read_file(root / "empty.lua");
  ASSERT_TRUE(empty);
  EXPECT_EQ(empty.value(), "");
  // Missing is not the same as empty.
  EXPECT_FALSE(sf::read_file(root / "missing.lua"));
}

TEST_F(ScriptFiles, InLibFolderLooksBelowTheFolderOnly) {
  EXPECT_TRUE(sf::in_lib_folder(root, root / "lib" / "helper.lua"));
  EXPECT_TRUE(sf::in_lib_folder(root, root / "sub" / "lib" / "x.lua"));
  EXPECT_FALSE(sf::in_lib_folder(root, root / "foo.lua"));
  EXPECT_FALSE(sf::in_lib_folder(root, root / "library" / "x.lua"));
  // The Linux package puts ${scripts} under /usr/lib: a `lib` above the
  // folder must not count, or every file is hidden.
  EXPECT_FALSE(sf::in_lib_folder("/usr/lib/nsclient/scripts/lua", "/usr/lib/nsclient/scripts/lua/foo.lua"));
}

TEST_F(ScriptFiles, ListFilesIsRelativeToScriptsAndSkipsLib) {
  std::list<std::string> files = sf::list_files(root, scripts, false);
  files.sort();
  std::list<std::string> expected = {(fs::path("lua") / "foo.lua").string(), (fs::path("lua") / "sub" / "bar.lua").string()};
  expected.sort();
  EXPECT_EQ(files, expected);

  std::list<std::string> with_lib = sf::list_files(root, scripts, true);
  EXPECT_EQ(with_lib.size(), 3u);
  EXPECT_NE(std::find(with_lib.begin(), with_lib.end(), (fs::path("lua") / "lib" / "helper.lua").string()), with_lib.end());
}

TEST_F(ScriptFiles, ListFilesOfAMissingFolderIsEmpty) { EXPECT_TRUE(sf::list_files(root / "missing", scripts, true).empty()); }
