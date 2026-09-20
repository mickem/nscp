// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <map>
#include <string>

#include <nscp/path_rooting.hpp>

// The rooting rule on its own, away from path_manager.
//
// path_manager_test already drives resolve_path end to end, but it can only
// reach the cases a real installation produces. The two decisions here are
// worth pinning directly because each one decides whether an operator's path
// gets relocated: names_a_root(), whose answer differs per platform by design,
// and the sentinels root_path() must hand back untouched.
namespace {

using nscp::paths::names_a_root;
using nscp::paths::path_expansion_error;
using nscp::paths::root_path;

// A stand-in for path_manager's expander: ${x} becomes the mapped value, and
// anything else is returned as-is. Deliberately not recursive - this test is
// about rooting, not about expansion.
struct fake_expander {
  std::map<std::string, std::string> tokens;
  mutable int calls = 0;
  std::string operator()(std::string value) const {
    ++calls;
    for (const auto &entry : tokens) {
      const std::string token = "${" + entry.first + "}";
      const size_t at = value.find(token);
      if (at != std::string::npos) value.replace(at, token.size(), entry.second);
    }
    return value;
  }
};

#ifdef WIN32
const char *const kAbsolute = "C:\\ProgramData\\NSClient++\\mine.log";
const char *const kRoot = "C:\\ProgramData\\NSClient++";
#else
const char *const kAbsolute = "/var/log/mine.log";
const char *const kRoot = "/var/log/nsclient";
#endif

// --- names_a_root -----------------------------------------------------------

TEST(PathRootingTest, ABareNameNamesNoRoot) { EXPECT_FALSE(names_a_root("mine.log")); }

TEST(PathRootingTest, ARelativeSubdirectoryNamesNoRoot) { EXPECT_FALSE(names_a_root("sub/mine.log")); }

TEST(PathRootingTest, AnUpwardRelativePathNamesNoRoot) { EXPECT_FALSE(names_a_root("../mine.log")); }

TEST(PathRootingTest, AnEmptyPathNamesNoRoot) { EXPECT_FALSE(names_a_root("")); }

TEST(PathRootingTest, AnAbsolutePathNamesARoot) { EXPECT_TRUE(names_a_root(kAbsolute)); }

#ifdef WIN32
// The reason this is not is_absolute(). `C:mine.log` is drive-relative, so
// is_absolute() says false - but the operator plainly named drive C, and
// joining a root onto it would produce the nonsense <root>\C:mine.log.
TEST(PathRootingTest, ADriveRelativePathNamesARootOnWindows) {
  EXPECT_FALSE(boost::filesystem::path("C:mine.log").is_absolute());
  EXPECT_TRUE(names_a_root("C:mine.log"));
}

// Root-relative: the drive is the caller's, the directory is not.
TEST(PathRootingTest, ARootRelativePathNamesARootOnWindows) { EXPECT_TRUE(names_a_root("\\logs\\mine.log")); }

TEST(PathRootingTest, AUncPathNamesARoot) { EXPECT_TRUE(names_a_root("\\\\server\\share\\mine.log")); }
#else
// The same string on unix is an ordinary relative file name - a file called
// `C:mine.log` in the current directory - and does get rooted. boost parses
// roots per platform, so this falls out rather than being special-cased.
TEST(PathRootingTest, ADriveRelativePathIsJustAFilenameOnUnix) { EXPECT_FALSE(names_a_root("C:mine.log")); }
#endif

// --- root_path: the values that must survive untouched ----------------------

TEST(PathRootingTest, AnEmptyValueStaysEmpty) {
  // "" means "not configured" for a good number of path options, and their
  // consumers test .empty(). Rooting it would invent a file named after a
  // folder.
  const fake_expander expand;
  EXPECT_EQ(root_path("", kRoot, expand), "");
}

TEST(PathRootingTest, TheNoPathSentinelIsNotRooted) {
  const fake_expander expand;
  EXPECT_EQ(root_path("none", kRoot, expand), "none");
}

TEST(PathRootingTest, TheSentinelIsMatchedExactlySoOtherSpellingsAreOrdinaryNames) {
  // is_no_path is a lower-case exact match on purpose: widening it would start
  // swallowing a file genuinely called `None` on a case-insensitive filesystem.
  const fake_expander expand;
  EXPECT_NE(root_path("None", kRoot, expand), "None");
  EXPECT_NE(root_path("none.log", kRoot, expand), "none.log");
}

// --- root_path: rooting ------------------------------------------------------

TEST(PathRootingTest, ABareNameIsRootedAtTheDefaultRoot) {
  const fake_expander expand;
  const boost::filesystem::path got(root_path("mine.log", kRoot, expand));
  EXPECT_EQ(got.parent_path(), boost::filesystem::path(kRoot));
  EXPECT_EQ(got.filename(), boost::filesystem::path("mine.log"));
}

TEST(PathRootingTest, AnAbsoluteValueIsLeftWhereTheOperatorPutIt) {
  const fake_expander expand;
  EXPECT_EQ(root_path(kAbsolute, kRoot, expand), kAbsolute);
}

TEST(PathRootingTest, ATokenExpandingToAnAbsolutePathIsNotRooted) {
  fake_expander expand;
  expand.tokens["log-path"] = kRoot;
  const std::string got = root_path("${log-path}/mine.log", kRoot, expand);
  EXPECT_TRUE(names_a_root(got));
  EXPECT_EQ(got, std::string(kRoot) + "/mine.log");
}

TEST(PathRootingTest, ATokenExpandingToABareNameIsStillRooted) {
  // Expansion is not absolutisation - the whole premise. A token whose value
  // is itself relative leaves a relative path, and that still has to land in
  // the consumer's folder.
  fake_expander expand;
  expand.tokens["leaf"] = "mine.log";
  const boost::filesystem::path got(root_path("${leaf}", kRoot, expand));
  EXPECT_EQ(got.parent_path(), boost::filesystem::path(kRoot));
}

TEST(PathRootingTest, TheRootIsExpandedTooSoItMayBeWrittenAsAToken) {
  fake_expander expand;
  expand.tokens["log-path"] = kRoot;
  const boost::filesystem::path got(root_path("mine.log", "${log-path}", expand));
  EXPECT_EQ(got.parent_path(), boost::filesystem::path(kRoot));
}

// --- root_path: a root that is not one ---------------------------------------

TEST(PathRootingTest, ARootThatResolvesToNothingIsReported) {
  fake_expander expand;
  expand.tokens["nowhere"] = "";
  EXPECT_THROW(root_path("mine.log", "${nowhere}", expand), path_expansion_error);
}

TEST(PathRootingTest, ARelativeRootIsReportedRatherThanUsed) {
  // Returning the value unrooted would leave it resolving against the working
  // directory, which is the failure this whole mechanism exists to remove - so
  // it is raised instead of limped past.
  const fake_expander expand;
  EXPECT_THROW(root_path("mine.log", "logs", expand), path_expansion_error);
}

TEST(PathRootingTest, TheErrorNamesBothTheValueAndTheRoot) {
  fake_expander expand;
  expand.tokens["bad-root"] = "logs";
  try {
    root_path("mine.log", "${bad-root}", expand);
    FAIL() << "expected path_expansion_error";
  } catch (const path_expansion_error &e) {
    const std::string what = e.what();
    EXPECT_NE(what.find("mine.log"), std::string::npos) << what;
    EXPECT_NE(what.find("${bad-root}"), std::string::npos) << what;
    // The resolved form too, since that is what makes a token's mistake legible.
    EXPECT_NE(what.find("'logs'"), std::string::npos) << what;
  }
}

TEST(PathRootingTest, ARootIsNotConsultedForAValueThatDoesNotNeedOne) {
  // An absolute value must not be able to fail on someone else's broken root.
  const fake_expander expand;
  EXPECT_NO_THROW(root_path(kAbsolute, "not-a-root", expand));
  EXPECT_EQ(root_path(kAbsolute, "not-a-root", expand), kAbsolute);
}

}  // namespace
