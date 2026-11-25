// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckLogFile module class: the settings it registers,
// the file access policy it builds from them, and check_logfile itself -
// argument validation, reading, max-lines / max-size, and bookmarks - over
// files the test writes to a fresh temporary directory.
//
// Everything loads with NSCAPI::dontStart, so the real-time monitor thread is
// constructed but never started. The bookmark state machine and the file
// reader have their own tests (bookmark_state_test, file_reader_test); this
// covers how the module drives them.

#include "CheckLogFile.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

const char *const kRoot = "/settings/logfile";

typedef PB::Commands::QueryResponseMessage::Response response_type;

class CheckLogFileModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
    dir_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("logfile-test-%%%%-%%%%");
    boost::filesystem::create_directories(dir_);
  }
  void TearDown() override {
    module_.unloadModule();
    core().reset();
    boost::system::error_code ec;
    boost::filesystem::remove_all(dir_, ec);
  }

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  std::string write(const std::string &name, const std::string &contents, const bool append = false) {
    const std::string path = (dir_ / name).string();
    std::ofstream out(path.c_str(), std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    out << contents;
    return path;
  }

  response_type check(const std::vector<std::string> &args) {
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command("check_logfile");
    for (const std::string &a : args) request.add_arguments(a);
    response_type response;
    module_.check_logfile(request, &response);
    return response;
  }

  static std::string message_of(const response_type &r) { return r.lines_size() > 0 ? r.lines(0).message() : std::string(); }

  boost::filesystem::path dir_;
  nscapi::test_helpers::stdout_capture out_;
  CheckLogFile module_;
};

// Three tab separated records, one of them an error.
const char *const kLog = "1\tINFO\tstarted\n2\tERROR\tdisk full\n3\tINFO\tstopped\n";

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(CheckLogFileModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for(std::string(kRoot) + "/real-time", "enabled"), "false");
  EXPECT_EQ(core().default_for(kRoot, "file access"), "any") << "open by default, so an upgrade changes nothing";
  EXPECT_EQ(core().default_for(kRoot, "allowed files"), "");
}

TEST_F(CheckLogFileModule, LoadRegistersTheSections) {
  ASSERT_TRUE(load());

  const std::vector<std::string> &paths = core().registered_paths();
  for (const std::string p : {std::string(kRoot) + "/real-time", std::string(kRoot) + "/real-time/checks", std::string(kRoot) + "/files"}) {
    EXPECT_NE(std::find(paths.begin(), paths.end(), p), paths.end()) << p;
  }
}

TEST_F(CheckLogFileModule, AnInvalidFileAccessModeIsReported) {
  core().set_setting(kRoot, "file access", "sometimes");

  ASSERT_TRUE(load());
  EXPECT_TRUE(core().has_logged("sometimes"));
}

// dontStart builds the monitor but must not start its thread; a reload stops
// the old one and builds a new one.
TEST_F(CheckLogFileModule, ReloadingWithoutStartingIsSafe) {
  ASSERT_TRUE(load());
  ASSERT_TRUE(load());
  EXPECT_FALSE(core().has_logged("Failed to start collection thread"));
}

// ============================================================================
// Argument validation
// ============================================================================

TEST_F(CheckLogFileModule, AFileIsRequired) {
  ASSERT_TRUE(load());
  const response_type r = check({});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(r), "Need to specify at least one file: file=foo.txt");
}

TEST_F(CheckLogFileModule, EmptySplitsAreRejected) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  EXPECT_EQ(message_of(check({"file=" + f, "column-split="})), "No column-split specified");
  EXPECT_EQ(message_of(check({"file=" + f, "line-split="})), "No line-split specified");
}

TEST_F(CheckLogFileModule, InvalidMaxSizeIsRejected) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  EXPECT_EQ(message_of(check({"file=" + f, "max-size=lots"})), "Invalid max-size: lots (expected a byte count, optionally suffixed k/m/g)");
}

TEST_F(CheckLogFileModule, InvalidNewestIsRejected) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  EXPECT_EQ(message_of(check({"file=" + f, "newest=middle"})), "Invalid newest: middle (expected first or last)");
}

TEST_F(CheckLogFileModule, NewestFirstCannotBeBookmarked) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  EXPECT_EQ(message_of(check({"file=" + f, "newest=first", "bookmark=auto"})),
            "newest=first cannot be combined with bookmark: a file which is rewritten from the top has no position to resume from");
}

TEST_F(CheckLogFileModule, AMissingFileIsReported) {
  ASSERT_TRUE(load());
  const std::string f = (dir_ / "missing.log").string();

  const response_type r = check({"file=" + f});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  // The path, then the reason the OS gave for it.
  EXPECT_EQ(message_of(r).rfind("Failed to open file: " + f + ": ", 0), 0u) << message_of(r);
}

// ============================================================================
// Reading
// ============================================================================

TEST_F(CheckLogFileModule, MatchingLinesDecideTheStatus) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  const response_type r = check({"file=" + f, "warning=column2 = 'ERROR'", "critical=column2 = 'FATAL'", "top-syntax=%(status): %(count)/%(total) %(list)",
                                 "detail-syntax=%(column3)", "show-all"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING) << message_of(r);
  EXPECT_NE(message_of(r).find("disk full"), std::string::npos) << message_of(r);
}

TEST_F(CheckLogFileModule, AFilterNarrowsTheRecords) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  const response_type r = check({"file=" + f, "filter=column2 = 'INFO'", "top-syntax=%(count)", "critical=count > 5"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK);
  EXPECT_EQ(message_of(r), "2");
}

// `file=` and `files=` are the same list: naming both reads both.
TEST_F(CheckLogFileModule, FileAndFilesAreCombined) {
  ASSERT_TRUE(load());
  const std::string a = write("a.log", kLog);
  const std::string b = write("b.log", "4\tERROR\tagain\n");

  const response_type r = check({"file=" + a, "files= " + b + " ,", "filter=column2 = 'ERROR'", "top-syntax=%(count)"});
  EXPECT_EQ(message_of(r), "2");
}

TEST_F(CheckLogFileModule, MaxLinesKeepsTheNewestLines) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  EXPECT_EQ(message_of(check({"file=" + f, "max-lines=1", "top-syntax=%(list)", "detail-syntax=%(column1)", "show-all"})), "3");
  EXPECT_EQ(message_of(check({"file=" + f, "max-lines=1", "newest=first", "top-syntax=%(list)", "detail-syntax=%(column1)", "show-all"})), "1");
}

TEST_F(CheckLogFileModule, CrlfLinesAreStripped) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", "1\tINFO\tTest 1\r\n2\tINFO\tTest 2\r\n");

  EXPECT_EQ(message_of(check({"file=" + f, "filter=column3 = 'Test 1'", "top-syntax=%(count)"})), "1");
}

TEST_F(CheckLogFileModule, CustomSplitsAreHonoured) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", "a,1|b,2|c,3");

  EXPECT_EQ(message_of(check({"file=" + f, "line-split=|", "split=,", "filter=column2 = '2'", "top-syntax=%(list)", "detail-syntax=%(column1)"})), "b");
}

// Without a bookmark there is no position to resume from: a file larger than
// max-size is refused rather than half read.
TEST_F(CheckLogFileModule, AFileOverMaxSizeWithoutABookmarkIsRefused) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  const std::string m = message_of(check({"file=" + f, "max-size=10"}));
  EXPECT_EQ(m.find("File is larger than max-size (10): " + f), 0u) << m;
  EXPECT_NE(m.find("bookmark=auto"), std::string::npos);
}

// ============================================================================
// Bookmarks
// ============================================================================

TEST_F(CheckLogFileModule, ABookmarkOnlyReportsNewLines) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);
  const std::vector<std::string> args = {"file=" + f, "bookmark=auto", "top-syntax=%(count)", "critical=count > 99"};

  EXPECT_EQ(message_of(check(args)), "3");
  EXPECT_EQ(message_of(check(args)), "OK: Nothing found") << "nothing new: the empty state";

  write("a.log", "4\tERROR\tnew\n", true);
  EXPECT_EQ(message_of(check(args)), "1");
}

// A trailing line with no terminator is held back until it is complete.
TEST_F(CheckLogFileModule, ABookmarkHoldsBackAnUnterminatedLine) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", "1\tINFO\tdone\n2\tINFO\tpart");
  const std::vector<std::string> args = {"file=" + f, "bookmark=pos", "top-syntax=%(count)", "critical=count > 99"};

  EXPECT_EQ(message_of(check(args)), "1");
  write("a.log", "ial\n", true);
  EXPECT_EQ(message_of(check(args)), "1");
}

// Two checks of one file with different expressions keep separate positions.
TEST_F(CheckLogFileModule, AutoBookmarksAreKeyedOnTheExpressions) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);

  EXPECT_EQ(message_of(check({"file=" + f, "bookmark", "filter=column2 = 'INFO'", "top-syntax=%(count)"})), "2");
  EXPECT_EQ(message_of(check({"file=" + f, "bookmark", "filter=column2 = 'ERROR'", "top-syntax=%(count)"})), "1");
  EXPECT_EQ(message_of(check({"file=" + f, "bookmark", "filter=column2 = 'INFO'", "top-syntax=%(count)"})), "OK: Nothing found");
}

// A rotated (replaced) file is read from the start again.
TEST_F(CheckLogFileModule, ARotatedFileIsReadFromTheStart) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", kLog);
  const std::vector<std::string> args = {"file=" + f, "bookmark=rot", "top-syntax=%(count)"};

  EXPECT_EQ(message_of(check(args)), "3");
  write("a.log", "9\tINFO\tfresh\n");
  EXPECT_EQ(message_of(check(args)), "1");
}

// With a bookmark max-size paces the read instead of refusing it: the backlog
// is worked through over several checks, a whole line at a time.
TEST_F(CheckLogFileModule, ABookmarkWorksThroughALargeBacklog) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", "aaaa\nbbbb\ncccc\n");
  const std::vector<std::string> args = {"file=" + f, "bookmark=big", "max-size=12", "top-syntax=%(count)"};

  EXPECT_EQ(message_of(check(args)), "2");
  EXPECT_EQ(message_of(check(args)), "1");
  EXPECT_EQ(message_of(check(args)), "OK: Nothing found");
}

TEST_F(CheckLogFileModule, ABookmarkWithALineLongerThanMaxSizeIsRefused) {
  ASSERT_TRUE(load());
  const std::string f = write("a.log", "a very long line\n");

  EXPECT_EQ(message_of(check({"file=" + f, "bookmark=big", "max-size=4"})),
            "No complete line in the first 4 of " + f + " (max-size): raise max-size or check line-split");
}

// ============================================================================
// File access
// ============================================================================

TEST_F(CheckLogFileModule, AllowedModeRefusesAFileOutsideTheList) {
  const boost::filesystem::path allowed = dir_ / "allowed";
  boost::filesystem::create_directories(allowed);
  core().set_setting(kRoot, "file access", "allowed");
  core().set_setting(kRoot, "allowed files", allowed.string());
  ASSERT_TRUE(load());
  const std::string inside = write("allowed/in.log", kLog);
  const std::string outside = write("out.log", kLog);

  EXPECT_EQ(message_of(check({"file=" + inside, "top-syntax=%(count)"})), "3");
  const response_type r = check({"file=" + outside});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(message_of(r).find(outside), std::string::npos) << message_of(r);
}

TEST_F(CheckLogFileModule, PredefinedModeOnlyAcceptsDefinedNames) {
  const std::string f = write("app.log", kLog);
  core().set_setting(kRoot, "file access", "predefined");
  core().set_keys("logfile/files", {{"app", f}});
  ASSERT_TRUE(load());

  EXPECT_EQ(message_of(check({"file=app", "top-syntax=%(count)"})), "3");
  EXPECT_EQ(check({"file=" + f}).result(), PB::Common::ResultCode::UNKNOWN) << "a path is not a predefined name";
}

// A reload re-reads the predefined names rather than appending to them.
TEST_F(CheckLogFileModule, ReloadReplacesThePredefinedNames) {
  const std::string f = write("app.log", kLog);
  core().set_setting(kRoot, "file access", "predefined");
  core().set_keys("logfile/files", {{"app", f}});
  ASSERT_TRUE(load());

  core().set_keys("logfile/files", {{"other", f}});
  ASSERT_TRUE(load());
  EXPECT_EQ(check({"file=app"}).result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(message_of(check({"file=other", "top-syntax=%(count)"})), "3");
}
