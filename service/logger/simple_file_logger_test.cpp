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
#include <nscapi/protobuf/log.hpp>
#include <sstream>
#include <string>

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
