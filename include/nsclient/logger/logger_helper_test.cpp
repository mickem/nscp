// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <iterator>
#include <nscapi/protobuf/log.hpp>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger_helper.hpp>
#include <string>
#include <vector>

#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// ============================================================================
// Tests for render_log_level_short
// ============================================================================

TEST(logger_helper, render_log_level_short_critical) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_CRITICAL), "C");
}

TEST(logger_helper, render_log_level_short_error) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_ERROR), "E");
}

TEST(logger_helper, render_log_level_short_warning) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_WARNING), "W");
}

TEST(logger_helper, render_log_level_short_info) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_INFO), "L");
}

TEST(logger_helper, render_log_level_short_debug) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_DEBUG), "D");
}

TEST(logger_helper, render_log_level_short_trace) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_short(PB::Log::LogEntry_Entry_Level_LOG_TRACE), "T");
}

// ============================================================================
// Tests for render_log_level_long
// ============================================================================

TEST(logger_helper, render_log_level_long_critical) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_CRITICAL), "critical");
}

TEST(logger_helper, render_log_level_long_error) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_ERROR), "error");
}

TEST(logger_helper, render_log_level_long_warning) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_WARNING), "warning");
}

TEST(logger_helper, render_log_level_long_info) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_INFO), "info");
}

TEST(logger_helper, render_log_level_long_debug) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_DEBUG), "debug");
}

TEST(logger_helper, render_log_level_long_trace) {
  EXPECT_EQ(nsclient::logging::logger_helper::render_log_level_long(PB::Log::LogEntry_Entry_Level_LOG_TRACE), "trace");
}

// ============================================================================
// Tests for render_console_message
// ============================================================================

TEST(logger_helper, render_console_message_basic) {
  std::string data = nsclient::logging::log_message_factory::create_info("test_module", "test.cpp", 42, "Test message");
  auto result = nsclient::logging::logger_helper::render_console_message(false, data);

  EXPECT_FALSE(result.first);  // not an error
  EXPECT_EQ(result.second, "L est_module Test message\n");
}

TEST(logger_helper, render_console_message_oneline) {
  std::string data = nsclient::logging::log_message_factory::create_info("test_module", "test.cpp", 42, "Test message");
  auto result = nsclient::logging::logger_helper::render_console_message(true, data);

  EXPECT_FALSE(result.first);  // not an error
  EXPECT_EQ(result.second, "test.cpp(42): info: Test message\n");
}

TEST(logger_helper, render_console_message_error_level) {
  std::string data = nsclient::logging::log_message_factory::create_error("test_module", "test.cpp", 42, "Error message");
  auto result = nsclient::logging::logger_helper::render_console_message(false, data);

  EXPECT_FALSE(result.first);
  EXPECT_EQ(result.second, "E est_module Error message\n                    test.cpp:42\n");
}

TEST(logger_helper, render_console_message_multiline) {
  std::string data = nsclient::logging::log_message_factory::create_info("module", "file.cpp", 100, "line1\nline2\nline3");
  auto result = nsclient::logging::logger_helper::render_console_message(false, data);

  EXPECT_FALSE(result.first);
  EXPECT_EQ(result.second, "L     module line1\nline2\nline3\n");
}

TEST(logger_helper, render_console_message_invalid_data) {
  std::string invalid_data = "this is not a valid protobuf message";
  auto result = nsclient::logging::logger_helper::render_console_message(false, invalid_data);

  EXPECT_TRUE(result.first);  // should be an error
  EXPECT_EQ(result.second, "ERROR");
}

TEST(logger_helper, render_console_message_empty_data) {
  std::string empty_data;
  auto result = nsclient::logging::logger_helper::render_console_message(false, empty_data);

  // Empty data should fail to parse
  EXPECT_TRUE(result.first);
  EXPECT_EQ(result.second, "ERROR");
}

TEST(logger_helper, render_console_message_all_levels_oneline) {
  // Test all log levels in oneline mode
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {nsclient::logging::log_message_factory::create_critical("mod", "f.cpp", 1, "msg"), "critical"},
      {nsclient::logging::log_message_factory::create_error("mod", "f.cpp", 1, "msg"), "error"},
      {nsclient::logging::log_message_factory::create_warning("mod", "f.cpp", 1, "msg"), "warning"},
      {nsclient::logging::log_message_factory::create_info("mod", "f.cpp", 1, "msg"), "info"},
      {nsclient::logging::log_message_factory::create_debug("mod", "f.cpp", 1, "msg"), "debug"},
      {nsclient::logging::log_message_factory::create_trace("mod", "f.cpp", 1, "msg"), "trace"},
  };

  for (const auto& test_case : test_cases) {
    auto result = nsclient::logging::logger_helper::render_console_message(true, test_case.first);
    EXPECT_FALSE(result.first);
    EXPECT_NE(result.second.find(test_case.second), std::string::npos) << "Expected level '" << test_case.second << "' in output: " << result.second;
  }
}

// ============================================================================
// Tests for get_formated_date
// ============================================================================

TEST(logger_helper, get_formated_date_basic) {
  std::string result = nsclient::logging::logger_helper::get_formated_date("%Y-%m-%d");
  EXPECT_FALSE(result.empty());
  // Should contain 4-digit year followed by dash
  EXPECT_NE(result.find('-'), std::string::npos);
}

TEST(logger_helper, get_formated_date_with_time) {
  std::string result = nsclient::logging::logger_helper::get_formated_date("%Y-%m-%d %H:%M:%S");
  EXPECT_FALSE(result.empty());
  // Should contain colons for time
  EXPECT_NE(result.find(':'), std::string::npos);
}

TEST(logger_helper, get_formated_date_year_only) {
  std::string result = nsclient::logging::logger_helper::get_formated_date("%Y");
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.length(), 4u);  // Just the year
}

TEST(logger_helper, get_formated_date_empty_format) {
  std::string result = nsclient::logging::logger_helper::get_formated_date("");
  // Empty format should return empty or default behavior
  // The exact behavior depends on implementation
}

// ============================================================================
// Tests for set_fatal_file / log_fatal
//
// log_fatal is the channel the terminate handler writes its report through,
// so what matters is that a report actually lands somewhere findable: the
// default put it in the working directory, which for a service is wherever
// the SCM happened to start it.
//
// Two things these cases have to be careful about, both of which bit earlier
// versions of them. The configured file is process-global, so every case runs
// under a fixture that puts it back - a test that left it pointing at a
// directory it had just deleted sent every later log_fatal in this binary
// somewhere nobody was looking. And the fallback keeps the file's name, so
// every case uses a name of its own: two cases sharing one name in a shared
// temp folder would both pass on a stale file left by an earlier run.
// ============================================================================

namespace {

class fatal_file : public ::testing::Test {
 protected:
  void SetUp() override { saved_ = nsclient::logging::logger_helper::fatal_file(); }

  void TearDown() override {
    nsclient::logging::logger_helper::set_fatal_file(saved_);
    for (const boost::filesystem::path &path : cleanup_) {
      boost::system::error_code ec;
      boost::filesystem::remove_all(path, ec);
    }
  }

  // A path that is removed when the test ends, however it ends.
  boost::filesystem::path scratch(const std::string &pattern = "nscp-fatal-%%%%%%%%") {
    const boost::filesystem::path path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path(pattern);
    cleanup_.push_back(path);
    return path;
  }

  void remove_when_done(const boost::filesystem::path &path) { cleanup_.push_back(path); }

  // A report file name no other case, and no earlier run, can be using.
  static std::string unique_report_name() { return boost::filesystem::unique_path("nscp-%%%%%%%%.fatal").string(); }

  static std::string read(const boost::filesystem::path &file) {
    std::ifstream stream(file.string().c_str());
    return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  }

 private:
  std::string saved_;
  std::vector<boost::filesystem::path> cleanup_;
};

}  // namespace

TEST_F(fatal_file, log_fatal_appends_to_the_configured_file) {
  const boost::filesystem::path dir = scratch();
  boost::filesystem::create_directories(dir);
  const boost::filesystem::path file = dir / unique_report_name();

  nsclient::logging::logger_helper::set_fatal_file(file.string());
  nsclient::logging::logger_helper::log_fatal("first report");
  nsclient::logging::logger_helper::log_fatal("second report");

  ASSERT_TRUE(boost::filesystem::exists(file));
  const std::string contents = read(file);
  EXPECT_NE(contents.find("first report"), std::string::npos);
  // Appended, not truncated: a crash loop must not erase the report from the
  // first crash, which is usually the informative one.
  EXPECT_NE(contents.find("second report"), std::string::npos);
}

TEST_F(fatal_file, set_fatal_file_ignores_an_empty_path) {
  const boost::filesystem::path dir = scratch();
  boost::filesystem::create_directories(dir);
  const boost::filesystem::path file = dir / unique_report_name();

  nsclient::logging::logger_helper::set_fatal_file(file.string());
  // An unset path setting must not silently send the report back to the
  // working directory - keep whatever was configured last.
  nsclient::logging::logger_helper::set_fatal_file("");
  EXPECT_EQ(nsclient::logging::logger_helper::fatal_file(), file.string());

  nsclient::logging::logger_helper::log_fatal("kept the configured file");
  ASSERT_TRUE(boost::filesystem::exists(file));
  EXPECT_NE(read(file).find("kept the configured file"), std::string::npos);
}

TEST_F(fatal_file, set_fatal_file_does_not_create_the_log_folder_up_front) {
  // This runs on every start, including every short-lived command line
  // invocation, so the probe must not leave a ${log-path} behind for a report
  // that may never be written - tests/fleet-sync-hostile.test.ts holds the
  // agent to creating nothing in its working directory it did not need to.
  // A folder that is not there yet is therefore accepted as-is.
  const boost::filesystem::path dir = scratch();
  const boost::filesystem::path file = dir / "logs" / unique_report_name();

  EXPECT_EQ(nsclient::logging::logger_helper::set_fatal_file(file.string()), file.string());
  EXPECT_FALSE(boost::filesystem::exists(dir));

  // ... and it is created at the moment there is something to write.
  nsclient::logging::logger_helper::log_fatal("report created the log folder");
  ASSERT_TRUE(boost::filesystem::exists(file));
  EXPECT_NE(read(file).find("report created the log folder"), std::string::npos);
}

TEST_F(fatal_file, set_fatal_file_leaves_no_empty_report_behind) {
  // An nsclient.fatal that exists means something was reported. An empty one
  // appearing on every boot would be a standing false alarm.
  const boost::filesystem::path dir = scratch();
  boost::filesystem::create_directories(dir);
  const boost::filesystem::path file = dir / unique_report_name();

  EXPECT_EQ(nsclient::logging::logger_helper::set_fatal_file(file.string()), file.string());
  EXPECT_FALSE(boost::filesystem::exists(file));
}

TEST_F(fatal_file, set_fatal_file_falls_back_to_a_private_temp_folder) {
  // A regular file where a directory is expected: create_directories() and
  // the open both fail, on every platform and regardless of the account the
  // process runs under (a root-owned CI container ignores mode bits).
  const boost::filesystem::path dir = scratch();
  boost::filesystem::create_directories(dir);
  const boost::filesystem::path blocker = dir / "not-a-directory";
  {
    std::ofstream make_blocker(blocker.string().c_str());
    make_blocker << "x";
  }
  const boost::filesystem::path unwritable = blocker / unique_report_name();

  const std::string expected = nsclient::logging::logger_helper::fatal_fallback_file(unwritable.string());
  ASSERT_FALSE(expected.empty());
  remove_when_done(expected);
  // The name is unique to this run, so the assertion below cannot be
  // satisfied by a leftover file.
  ASSERT_FALSE(boost::filesystem::exists(expected));
  // Not the shared temp folder itself: that one is world-writable, and a
  // service appending to a fixed name there as root is a file-overwrite
  // primitive.
  EXPECT_NE(boost::filesystem::path(expected).parent_path(), boost::filesystem::temp_directory_path());

  EXPECT_EQ(nsclient::logging::logger_helper::set_fatal_file(unwritable.string()), expected);

  nsclient::logging::logger_helper::log_fatal("fell back to the temp folder");
  ASSERT_TRUE(boost::filesystem::exists(expected));
  EXPECT_NE(read(expected).find("fell back to the temp folder"), std::string::npos);
}

#ifndef WIN32
TEST_F(fatal_file, the_fallback_folder_is_private_to_this_account) {
  const std::string fallback = nsclient::logging::logger_helper::fatal_fallback_file("nsclient.fatal");
  ASSERT_FALSE(fallback.empty());
  const std::string dir = boost::filesystem::path(fallback).parent_path().string();

  struct stat st;
  ASSERT_EQ(::lstat(dir.c_str(), &st), 0);
  EXPECT_TRUE(S_ISDIR(st.st_mode)) << dir << " must be a directory, not a symlink to one";
  EXPECT_EQ(st.st_uid, ::geteuid());
  EXPECT_EQ(st.st_mode & (S_IRWXG | S_IRWXO), 0u) << "the crash report folder must not be reachable by other accounts";
}
#endif

TEST_F(fatal_file, log_fatal_falls_back_when_the_folder_cannot_be_restored) {
  // The folder was writable when it was configured and is not any more - and
  // creating it again is not the answer either, because something else now
  // sits where it was. The classic case is simple_file_logger reporting
  // "Failed to create log directory" through this very channel, into the
  // directory it just failed to create.
  const boost::filesystem::path dir = scratch();
  boost::filesystem::create_directories(dir);
  const boost::filesystem::path file = dir / unique_report_name();
  ASSERT_EQ(nsclient::logging::logger_helper::set_fatal_file(file.string()), file.string());

  boost::filesystem::remove_all(dir);
  {
    std::ofstream blocker(dir.string().c_str());
    blocker << "x";
  }

  const std::string expected = nsclient::logging::logger_helper::fatal_fallback_file(file.string());
  ASSERT_FALSE(expected.empty());
  remove_when_done(expected);
  ASSERT_FALSE(boost::filesystem::exists(expected));

  nsclient::logging::logger_helper::log_fatal("the log folder vanished");

  ASSERT_TRUE(boost::filesystem::exists(expected));
  EXPECT_NE(read(expected).find("the log folder vanished"), std::string::npos);
}
