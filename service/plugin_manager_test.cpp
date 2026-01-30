#include "plugin_manager.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <nsclient/logger/logger.hpp>

// Mock logger
class mock_logger : public nsclient::logging::log_interface {
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

// Mock loader

nscapi::core_api::FUNPTR NSAPILoader(const char *buffer) {
  return nullptr;
}

#ifdef WIN32
// Mock error lookup
namespace error {
namespace win32 {
unsigned int lookup() { return 0; }
std::string format_message(unsigned long attrs, std::string module, unsigned long dwError) { return ""; }
}
}
#endif


TEST(plugin_manager_test, test_is_module) {
    boost::filesystem::path dll_path("test.dll");
    boost::filesystem::path so_path("test.so");
    boost::filesystem::path txt_path("test.txt");

#ifdef WIN32
    EXPECT_TRUE(nsclient::core::plugin_manager::is_module(dll_path));
    EXPECT_FALSE(nsclient::core::plugin_manager::is_module(so_path));
#else
    EXPECT_FALSE(nsclient::core::plugin_manager::is_module(dll_path));
    EXPECT_TRUE(nsclient::core::plugin_manager::is_module(so_path));
#endif
    EXPECT_FALSE(nsclient::core::plugin_manager::is_module(txt_path));
}

TEST(plugin_manager_test, test_file_to_module) {
    boost::filesystem::path dll_path("test.dll");
    boost::filesystem::path so_path("libtest.so");
    boost::filesystem::path simple_so_path("test.so");

#ifdef WIN32
    EXPECT_EQ(nsclient::core::plugin_manager::file_to_module(dll_path), "test");
#else
    EXPECT_EQ(nsclient::core::plugin_manager::file_to_module(so_path), "test");
    EXPECT_EQ(nsclient::core::plugin_manager::file_to_module(simple_so_path), "test");
#endif
}
