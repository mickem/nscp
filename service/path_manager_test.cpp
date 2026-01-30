#include "path_manager.hpp"

#include <gtest/gtest.h>

#include <boost/make_shared.hpp>
#include <nsclient/logger/logger.hpp>

class MockLogger : public nsclient::logging::log_interface {
 public:
  void trace(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void debug(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void info(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void warning(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void error(const std::string &module, const char *file, const int line, const std::string &message) override {}
  void critical(const std::string &module, const char *file, const int line, const std::string &message) override {}

  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return false; }
  bool should_critical() const override { return false; }
};

class PathManagerTest : public ::testing::Test {
 protected:
  nsclient::logging::log_client_accessor log_instance_;
  nsclient::core::path_manager *pm;

  void SetUp() override {
    log_instance_ = boost::make_shared<MockLogger>();
    pm = new nsclient::core::path_manager(log_instance_);
  }

  void TearDown() override { delete pm; }
};

TEST_F(PathManagerTest, ExpandPathEmpty) { EXPECT_EQ(pm->expand_path(""), ""); }

TEST_F(PathManagerTest, ExpandPathNoVariables) {
  std::string path = "/usr/local/bin";
  EXPECT_EQ(pm->expand_path(path), path);
}

TEST_F(PathManagerTest, GetFolderUnknownKey) {
  std::string key = "unknown-key";
  std::string result = pm->getFolder(key);
  EXPECT_FALSE(result.empty());
}

TEST_F(PathManagerTest, ExpandPathWithVariables) {
  std::string input = "${base-path}/config";
  std::string output = pm->expand_path(input);
  EXPECT_NE(output, input);
  EXPECT_TRUE(output.find("config") != std::string::npos);
}

TEST_F(PathManagerTest, GetFolderKeys) {
  const char *keys[] = {"certificate-path", "module-path", "web-path", "scripts",     "log-path", "cache-folder",
                        "crash-folder",     "base-path",   "temp",     "shared-path", "exe-path", "data-path"};

  for (const auto &key : keys) {
    EXPECT_FALSE(pm->getFolder(key).empty()) << "Failed for key: " << key;
  }
}

#ifdef WIN32
TEST_F(PathManagerTest, GetFolderKeysWindows) {
  const char *keys[] = {"common-appdata", "appdata"};

  for (const auto &key : keys) {
    EXPECT_FALSE(pm->getFolder(key).empty()) << "Failed for key: " << key;
  }
}
#else
TEST_F(PathManagerTest, GetFolderKeysLinux) { EXPECT_EQ(pm->getFolder("etc"), "/etc"); }
#endif
