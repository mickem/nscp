// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for nsclient::logging::impl::simple_file_logger.
 *
 * The file path used by simple_file_logger is built as
 *     base_path() + <constructor argument>
 * where base_path() is the directory containing the running executable on
 * Windows (no trailing separator!) and the empty string on POSIX. Since the
 * file_ member is private and there is no setter outside of asynch_configure
 * (which goes through the settings manager) the tests below mirror the same
 * concatenation when computing where the test file ends up.
 *
 * Each test uses a unique file name that includes the pid + a counter so
 * concurrent test processes don't fight over the same file.
 */

#include "simple_file_logger.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <boost/filesystem.hpp>
#include <fstream>
#include <map>
#include <memory>
#include <nscapi/protobuf/log.hpp>
#include <settings/test_helpers.hpp>
#include <sstream>
#include <string>

#include "../libs/settings_manager/settings_manager_impl.h"

#ifdef WIN32
#include <process.h>
#define LFL_GETPID _getpid
#else
#include <unistd.h>
#define LFL_GETPID getpid
#endif

#ifdef WIN32
#include <win/shellapi.hpp>
#endif

using nsclient::logging::impl::simple_file_logger;

namespace {

std::string base_dir() {
#ifdef WIN32
  return shellapi::get_module_file_name().string();
#else
  return "";
#endif
}

std::string unique_name(const std::string& tag) {
  static std::atomic<int> counter{0};
  // The leading separator ensures the resulting path is well-formed on Windows
  // (where base_path() returns the exe directory without a trailing slash) and
  // a relative path on POSIX (current working directory).
#ifdef WIN32
  const std::string sep = "\\";
#else
  const std::string sep = "";
#endif
  std::ostringstream oss;
  oss << sep << "simple_file_logger_test_" << tag << "_" << LFL_GETPID() << "_" << counter.fetch_add(1) << ".log";
  return oss.str();
}

std::string expected_path(const std::string& name) { return base_dir() + name; }

class TempFile {
 public:
  explicit TempFile(std::string name) : name_(std::move(name)), full_path_(expected_path(name_)) { remove(); }
  ~TempFile() { remove(); }
  const std::string& name() const { return name_; }
  const std::string& full_path() const { return full_path_; }
  bool exists() const { return boost::filesystem::exists(full_path_); }
  std::uintmax_t size() const { return boost::filesystem::file_size(full_path_); }
  std::string read() const {
    std::ifstream ifs(full_path_, std::ios::binary);
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
  }
  void remove() {
    boost::system::error_code ec;
    boost::filesystem::remove(full_path_, ec);  // ignore errors
  }

 private:
  std::string name_;
  std::string full_path_;
};

// Build a serialized PB::Log::LogEntry (single entry).
std::string make_entry(PB::Log::LogEntry::Entry::Level level, const std::string& sender, const std::string& file, int line, const std::string& message) {
  PB::Log::LogEntry msg;
  auto* e = msg.add_entry();
  e->set_sender(sender);
  e->set_level(level);
  e->set_file(file);
  e->set_line(line);
  e->set_message(message);
  return msg.SerializeAsString();
}

}  // namespace

TEST(SimpleFileLogger, ShutdownReturnsTrue) {
  simple_file_logger logger(unique_name("shutdown"));
  EXPECT_TRUE(logger.shutdown());
}

TEST(SimpleFileLogger, BasePathOnPosixIsEmpty) {
#ifndef WIN32
  simple_file_logger logger("dummy");
  EXPECT_EQ(logger.base_path(), "");
#else
  GTEST_SKIP() << "base_path() returns the exe directory on Windows";
#endif
}

TEST(SimpleFileLogger, DoLogWritesFormattedEntryToFile) {
  TempFile file(unique_name("write"));
  ASSERT_FALSE(file.exists());

  simple_file_logger logger(file.name());
  const auto payload = make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "test", "file.cpp", 42, "hello world");
  logger.do_log(payload);

  ASSERT_TRUE(file.exists()) << "expected file at " << file.full_path();
  const std::string contents = file.read();
  EXPECT_NE(contents.find("hello world"), std::string::npos);
  EXPECT_NE(contents.find("info"), std::string::npos);  // long-form level
  EXPECT_NE(contents.find("file.cpp"), std::string::npos);
  EXPECT_NE(contents.find("42"), std::string::npos);
}

TEST(SimpleFileLogger, DoLogAppendsAcrossCalls) {
  TempFile file(unique_name("append"));
  simple_file_logger logger(file.name());
  logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "t", "f", 1, "first-line"));
  logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "t", "f", 2, "second-line"));

  ASSERT_TRUE(file.exists());
  const std::string contents = file.read();
  EXPECT_NE(contents.find("first-line"), std::string::npos);
  EXPECT_NE(contents.find("second-line"), std::string::npos);
}

TEST(SimpleFileLogger, DoLogIsSafeOnMalformedPayload) {
  TempFile file(unique_name("malformed"));
  simple_file_logger logger(file.name());
  // Should not crash; the parse failure is logged via log_fatal which writes
  // to cout and the "nsclient.fatal" file - we just make sure no exception
  // escapes here.
  logger.do_log("not a protobuf at all");
  SUCCEED();
}

TEST(SimpleFileLogger, DoLogIsSafeOnEmptyPayload) {
  TempFile file(unique_name("empty"));
  simple_file_logger logger(file.name());
  logger.do_log("");
  SUCCEED();
}

TEST(SimpleFileLogger, ConfigureMethodsDoNotThrow) {
  // synch_configure / asynch_configure go through the settings registry; in
  // a unit test no settings backend is configured, so they fail internally
  // and either swallow the error (asynch_configure) or log fatal (synch).
  // The contract under test is "they do not throw".
  simple_file_logger logger(unique_name("configure"));
  EXPECT_NO_THROW(logger.asynch_configure());
  EXPECT_NO_THROW(logger.synch_configure());
}

// ---------------------------------------------------------------------------
// Configuration through a real settings store. asynch_configure reads
// [/settings/log] (file name, date format) and [/settings/log/file] (max
// size) through the global settings manager, so these tests boot one against
// an INI file in a temp dir.
// ---------------------------------------------------------------------------

namespace {

class test_provider : public settings_manager::provider_interface {
 public:
  test_provider() : logger_(settings_test::make_null_logger()) {}
  std::string expand_path(std::string file) override { return file; }
  nsclient::logging::logger_instance get_logger() const override { return logger_; }
  void apply_path_overrides(std::map<std::string, std::string>) override {}

 private:
  nsclient::logging::logger_instance logger_;
};

// Enters a directory for the duration of a test and restores the previous
// working directory afterwards, whatever the test does.
class cwd_guard {
 public:
  explicit cwd_guard(const boost::filesystem::path& to) : previous_(boost::filesystem::current_path()) { boost::filesystem::current_path(to); }
  ~cwd_guard() {
    boost::system::error_code ignored;
    boost::filesystem::current_path(previous_, ignored);
  }

 private:
  boost::filesystem::path previous_;
};

class SimpleFileLoggerSettingsTest : public ::testing::Test {
 protected:
  settings_test::temp_dir dir_;
  std::unique_ptr<test_provider> provider_;

  void SetUp() override {
    settings_manager::destroy_settings();
    provider_ = std::make_unique<test_provider>();
  }

  void TearDown() override { settings_manager::destroy_settings(); }

  // Boot the global settings manager against an INI file with the given body.
  void boot_with(const std::string& body) {
    const boost::filesystem::path ini = dir_.file("settings.ini");
    settings_test::write_file(ini, body);
    std::string context = ini.generic_string();
    if (!context.empty() && context.front() == '/') context.erase(0, 1);
    ASSERT_TRUE(settings_manager::init_settings(provider_.get(), "ini:///" + context));
  }

  static std::string read_all(const boost::filesystem::path& p) {
    std::ifstream ifs(p.string().c_str(), std::ios::binary);
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
  }
};

}  // namespace

TEST_F(SimpleFileLoggerSettingsTest, AsynchConfigureAppliesFileNameAndDateFormat) {
  const boost::filesystem::path target = dir_.path() / "configured.log";
  boot_with(
      "[/settings/log]\n"
      "file name = " + target.generic_string() + "\n"
      "date format = DATEMARK\n");

  simple_file_logger logger(unique_name("ignored"));
  logger.asynch_configure();
  logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "t", "f", 3, "configured-message"));

  ASSERT_TRUE(boost::filesystem::exists(target)) << target;
  const std::string contents = read_all(target);
  EXPECT_NE(contents.find("configured-message"), std::string::npos);
  // The date format is applied verbatim (no % tokens here), proving the value
  // travelled from the store into the formatter.
  EXPECT_NE(contents.find("DATEMARK: "), std::string::npos) << contents;
}

TEST_F(SimpleFileLoggerSettingsTest, SynchConfigureReadsTheSameConfiguration) {
  boot_with(
      "[/settings/log]\n"
      "file name = " + (dir_.path() / "synch.log").generic_string() + "\n");

  simple_file_logger logger(unique_name("synch"));
  EXPECT_NO_THROW(logger.synch_configure());
}

// The filesystem edge cases below drive the target through the settings store
// rather than the constructor: a configured name that already carries a path
// separator is used verbatim, whereas the constructor always prepends
// base_path() - which is the executable's directory on Windows, so an absolute
// path handed to it would be mangled there.
TEST_F(SimpleFileLoggerSettingsTest, DoLogCreatesMissingParentDirectories) {
  const boost::filesystem::path target = dir_.path() / "logs" / "nested" / "output.log";
  boot_with(
      "[/settings/log]\n"
      "file name = " + target.generic_string() + "\n");

  simple_file_logger nested(unique_name("nested"));
  nested.asynch_configure();
  nested.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "t", "f", 7, "deep-message"));

  ASSERT_TRUE(boost::filesystem::exists(target)) << target;
  EXPECT_NE(read_all(target).find("deep-message"), std::string::npos);
}

TEST_F(SimpleFileLoggerSettingsTest, DoLogSurvivesAnUncreatableParentDirectory) {
  // The parent path runs through a regular file, so create_directories cannot
  // succeed and neither can the open; both failures must stay inside do_log.
  settings_test::write_file(dir_.file("blocker"), "a file, not a directory");
  const boost::filesystem::path target = dir_.path() / "blocker" / "sub" / "output.log";
  boot_with(
      "[/settings/log]\n"
      "file name = " + target.generic_string() + "\n");

  simple_file_logger logger(unique_name("blocked"));
  logger.asynch_configure();
  EXPECT_NO_THROW(logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_ERROR, "t", "f", 1, "lost")));
  EXPECT_FALSE(boost::filesystem::exists(target));
}

TEST_F(SimpleFileLoggerSettingsTest, DoLogSurvivesATargetThatIsADirectory) {
  // The log file name points at an existing directory: the stream cannot open
  // and the entry is diverted to the fatal log instead of crashing the logger.
  const boost::filesystem::path target = dir_.path() / "a-directory";
  boost::filesystem::create_directories(target);
  boot_with(
      "[/settings/log]\n"
      "file name = " + target.generic_string() + "\n");

  simple_file_logger logger(unique_name("dir"));
  logger.asynch_configure();
  EXPECT_NO_THROW(logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_ERROR, "t", "f", 1, "nowhere to go")));
}

TEST_F(SimpleFileLoggerSettingsTest, MaxSizeTruncatesTheLogFile) {
  const boost::filesystem::path target = dir_.path() / "rotate.log";
  boot_with(
      "[/settings/log]\n"
      "file name = " + target.generic_string() + "\n"
      "[/settings/log/file]\n"
      "max size = 400\n");

  simple_file_logger logger(unique_name("rotate"));
  logger.asynch_configure();

  const std::string padding(80, 'x');
  for (int i = 0; i < 6; i++) {
    logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "t", "f", i, "entry-" + std::to_string(i) + "-" + padding));
  }
  ASSERT_TRUE(boost::filesystem::exists(target));
  // Six ~120 byte lines against a 400 byte cap: the file must have been cut
  // back (to 70% of the cap before the last append), so the oldest entry is
  // gone while the newest survived.
  EXPECT_LT(boost::filesystem::file_size(target), 700u);
  const std::string contents = read_all(target);
  EXPECT_NE(contents.find("entry-5-"), std::string::npos);
  EXPECT_EQ(contents.find("entry-0-"), std::string::npos) << "the oldest entry survived truncation";
}

TEST_F(SimpleFileLoggerSettingsTest, FileNameNoneDisablesTheFileLog) {
  boot_with(
      "[/settings/log]\n"
      "file name = none\n");

  simple_file_logger logger(unique_name("disabled"));
  logger.asynch_configure();
  EXPECT_NO_THROW(logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "t", "f", 1, "dropped")));
  EXPECT_FALSE(boost::filesystem::exists("none"));
}

#ifndef WIN32
TEST_F(SimpleFileLoggerSettingsTest, ABareFileNameLandsNextToTheBinary) {
  // No path separator in the configured name: base_path() (empty on POSIX,
  // i.e. the working directory) is prepended. Run from inside the temp dir so
  // the resolved-relative-to-cwd behaviour is exercised without depending on
  // the working directory the suite happened to be started in being writable
  // (it is not, for instance, when ctest runs from a read-only tree).
  const cwd_guard cwd(dir_.path());
  const std::string name = "simple_file_logger_bare.log";
  boot_with(
      "[/settings/log]\n"
      "file name = " + name + "\n");

  simple_file_logger logger(unique_name("bare"));
  logger.asynch_configure();
  logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "t", "f", 1, "bare-name"));

  EXPECT_TRUE(boost::filesystem::exists(dir_.path() / name));
}
#endif

TEST_F(SimpleFileLoggerSettingsTest, ARotatedTargetThatIsADirectoryIsSurvived) {
  // With a max size configured, do_log stats the target before writing; a
  // directory in its place makes file_size() throw, which must be contained.
  const boost::filesystem::path target = dir_.path() / "actually-a-dir";
  boost::filesystem::create_directories(target);
  boot_with(
      "[/settings/log]\n"
      "file name = " + target.generic_string() + "\n"
      "[/settings/log/file]\n"
      "max size = 100\n");

  simple_file_logger logger(unique_name("dir-target"));
  logger.asynch_configure();
  EXPECT_NO_THROW(logger.do_log(make_entry(PB::Log::LogEntry_Entry_Level_LOG_INFO, "t", "f", 1, "contained")));
}
