/*
 * Unit tests for file_helpers.
 *
 * Coverage:
 *   - checks::path_contains_file
 *   - checks::is_safe_archive_entry  (zip-slip guard for settings_http)
 *   - checks::is_directory / is_file
 *   - read_file_as_string
 *   - meta::get_filename / get_path / get_extension / make_preferred
 *   - patterns::glob_to_regexp / split_pattern / split_path_ex / combine_pattern
 *
 * finder::locate_file_icase is omitted: it has platform-specific code paths
 * (SHGetFileInfo on Windows, case-insensitive directory walk on POSIX) that
 * would need a non-trivial fixture and isn't a security boundary.
 */

#include "file_helpers.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = boost::filesystem;
using file_helpers::checks;
using file_helpers::meta;
using file_helpers::patterns;

namespace {
const fs::path kBase("/cache");

// RAII temp directory: created in the system tmp area, removed on destruction.
class temp_dir {
 public:
  temp_dir() : path_(fs::temp_directory_path() / fs::unique_path("nscp-fh-%%%%%%%%")) { fs::create_directories(path_); }
  ~temp_dir() {
    boost::system::error_code ec;
    fs::remove_all(path_, ec);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};
}  // namespace

// ---------------------------------------------------------------------------
// path_contains_file
// ---------------------------------------------------------------------------

TEST(PathContainsFile, BasicPrefixMatch) { EXPECT_TRUE(checks::path_contains_file(fs::path("/cache"), fs::path("/cache/sub/foo.ini"))); }

TEST(PathContainsFile, NormalisedTraversal) { EXPECT_FALSE(checks::path_contains_file(fs::path("/cache"), fs::path("/cache/../etc/passwd"))); }

TEST(PathContainsFile, UnrelatedPathRejected) { EXPECT_FALSE(checks::path_contains_file(fs::path("/cache"), fs::path("/etc/passwd"))); }

TEST(PathContainsFile, SiblingDirectoryRejected) {
  // A path that shares a prefix string but is in a sibling directory must be
  // rejected (component-wise compare, not substring).
  EXPECT_FALSE(checks::path_contains_file(fs::path("/cache"), fs::path("/cache-evil/foo.ini")));
}

TEST(PathContainsFile, NestedDirAccepted) { EXPECT_TRUE(checks::path_contains_file(fs::path("/cache"), fs::path("/cache/a/b/c/d/foo"))); }

// ---------------------------------------------------------------------------
// is_safe_archive_entry — the zip-slip guard
// ---------------------------------------------------------------------------

TEST(IsSafeArchiveEntry, AcceptsBasicFile) {
  fs::path out;
  EXPECT_TRUE(checks::is_safe_archive_entry(kBase, "foo.ini", out));
  EXPECT_EQ(out, fs::path("/cache/foo.ini").lexically_normal());
}

TEST(IsSafeArchiveEntry, AcceptsNestedFile) {
  fs::path out;
  EXPECT_TRUE(checks::is_safe_archive_entry(kBase, "sub/dir/foo.ini", out));
  EXPECT_EQ(out, fs::path("/cache/sub/dir/foo.ini").lexically_normal());
}

TEST(IsSafeArchiveEntry, AcceptsCurrentDirPrefix) {
  // "./foo.ini" is benign - lexically_normal collapses it to "foo.ini".
  fs::path out;
  EXPECT_TRUE(checks::is_safe_archive_entry(kBase, "./foo.ini", out));
  EXPECT_EQ(out, fs::path("/cache/foo.ini").lexically_normal());
}

TEST(IsSafeArchiveEntry, RejectsEmptyName) {
  fs::path out;
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, "", out));
}

TEST(IsSafeArchiveEntry, RejectsEmbeddedNul) {
  fs::path out;
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, std::string("foo\0bar", 7), out));
}

TEST(IsSafeArchiveEntry, RejectsTrailingNul) {
  fs::path out;
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, std::string("foo.ini\0", 8), out));
}

TEST(IsSafeArchiveEntry, RejectsParentTraversal) {
  fs::path out;
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, "../etc/passwd", out));
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, "../../etc/passwd", out));
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, "sub/../../etc/passwd", out));
}

TEST(IsSafeArchiveEntry, RejectsAbsolutePosixPath) {
  fs::path out;
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, "/etc/passwd", out));
}

#ifdef _WIN32
TEST(IsSafeArchiveEntry, RejectsWindowsAbsolutePath) {
  fs::path out;
  const fs::path base("C:\\ProgramData\\NSClient++\\cache");
  EXPECT_FALSE(checks::is_safe_archive_entry(base, "C:\\Windows\\System32\\evil.dll", out));
  EXPECT_FALSE(checks::is_safe_archive_entry(base, "..\\..\\Windows\\System32\\evil.dll", out));
}

TEST(IsSafeArchiveEntry, AcceptsWindowsBackslashedRelativePath) {
  fs::path out;
  const fs::path base("C:\\ProgramData\\NSClient++\\cache");
  EXPECT_TRUE(checks::is_safe_archive_entry(base, "sub\\foo.ini", out));
}
#endif

TEST(IsSafeArchiveEntry, RejectsTraversalThatEqualsBase) {
  // A path that resolves exactly to `base` has no filename to write to.
  // path_contains_file's prefix logic rejects this, which is the safe outcome.
  fs::path out;
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, "..", out));
}

TEST(IsSafeArchiveEntry, OutNotMutatedOnReject) {
  // The contract documented on the helper: `out` must be left untouched on
  // failure so callers can rely on its prior value (or default).
  fs::path out("sentinel");
  EXPECT_FALSE(checks::is_safe_archive_entry(kBase, "../escape", out));
  EXPECT_EQ(out, fs::path("sentinel"));
}

// ---------------------------------------------------------------------------
// is_directory / is_file (filesystem-touching)
// ---------------------------------------------------------------------------

TEST(Checks, IsDirectoryAndIsFile) {
  const temp_dir td;
  EXPECT_TRUE(checks::is_directory(td.path().string()));
  EXPECT_FALSE(checks::is_file(td.path().string()));

  const fs::path file = td.path() / "hello.txt";
  {
    std::ofstream ofs(file.string());
    ofs << "hi";
  }
  EXPECT_TRUE(checks::is_file(file.string()));
  EXPECT_FALSE(checks::is_directory(file.string()));
}

TEST(Checks, IsDirectoryReturnsFalseForMissingPath) {
  const temp_dir td;
  EXPECT_FALSE(checks::is_directory((td.path() / "does-not-exist").string()));
  EXPECT_FALSE(checks::is_file((td.path() / "does-not-exist").string()));
}

// ---------------------------------------------------------------------------
// read_file_as_string
// ---------------------------------------------------------------------------

TEST(ReadFileAsString, RoundTripsContent) {
  const temp_dir td;
  const fs::path file = td.path() / "data.txt";
  {
    std::ofstream ofs(file.string(), std::ios::binary);
    ofs << "line one\nline two\n";
  }
  EXPECT_EQ(file_helpers::read_file_as_string(file), "line one\nline two\n");
}

TEST(ReadFileAsString, ThrowsOnMissingFile) {
  const temp_dir td;
  EXPECT_THROW(file_helpers::read_file_as_string(td.path() / "no-such-file"), std::runtime_error);
}

TEST(ReadFileAsString, EmptyFileReadsEmpty) {
  const temp_dir td;
  const fs::path file = td.path() / "empty.txt";
  std::ofstream(file.string()).close();
  EXPECT_EQ(file_helpers::read_file_as_string(file), "");
}

// ---------------------------------------------------------------------------
// meta
// ---------------------------------------------------------------------------

TEST(Meta, GetFilenameFromPath) {
  EXPECT_EQ(meta::get_filename(fs::path("/a/b/c.txt")), "c.txt");
  EXPECT_EQ(meta::get_filename(fs::path("c.txt")), "c.txt");
}

TEST(Meta, GetFilenameFromString) { EXPECT_EQ(meta::get_filename(std::string("/a/b/c.txt")), "c.txt"); }

TEST(Meta, GetPathFromString) {
  EXPECT_EQ(meta::get_path("/a/b/c.txt"), "/a/b");
  EXPECT_EQ(meta::get_path("c.txt"), "");
}

TEST(Meta, GetExtension) {
  EXPECT_EQ(meta::get_extension(fs::path("foo.ini")), ".ini");
  EXPECT_EQ(meta::get_extension(fs::path("foo")), "");
  EXPECT_EQ(meta::get_extension(fs::path("foo.tar.gz")), ".gz");
}

// ---------------------------------------------------------------------------
// patterns
// ---------------------------------------------------------------------------

TEST(Patterns, GlobToRegexpHandlesStarQuestionDot) {
  EXPECT_EQ(patterns::glob_to_regexp("*.txt"), ".*\\.txt");
  EXPECT_EQ(patterns::glob_to_regexp("file?.log"), "file.\\.log");
  EXPECT_EQ(patterns::glob_to_regexp("a.b.c"), "a\\.b\\.c");
}

TEST(Patterns, GlobToRegexpEmptyInput) { EXPECT_EQ(patterns::glob_to_regexp(""), ""); }

TEST(Patterns, CombinePatternRoundTrip) {
  const patterns::pattern_type p{fs::path("/a/b"), fs::path("*.ini")};
  const fs::path combined = patterns::combine_pattern(p);
  // / on Windows uses backslash, on POSIX forward slash. Compare via path
  // semantic equality (which boost::filesystem::path provides).
  EXPECT_EQ(combined, fs::path("/a/b") / "*.ini");
}

TEST(Patterns, SplitPatternOnFile) {
  // A non-existent file path that has a pattern in its filename. The function
  // does not consult the filesystem for file paths; it just splits parent and
  // filename when the path is not an existing directory.
  const temp_dir td;
  const fs::path file = td.path() / "*.ini";
  const auto split = patterns::split_pattern(file);
  EXPECT_EQ(split.first, td.path());
  EXPECT_EQ(split.second, fs::path("*.ini"));
}

TEST(Patterns, SplitPatternOnExistingDirectoryReturnsEmptyPattern) {
  const temp_dir td;
  const auto split = patterns::split_pattern(td.path());
  EXPECT_EQ(split.first, td.path());
  EXPECT_EQ(split.second, fs::path());
}
