#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <win/shellapi.hpp>

#ifdef WIN32

TEST(ShellApiTest, GetModuleFileName) {
  const boost::filesystem::path path = shellapi::get_module_file_name();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(boost::filesystem::exists(path));
}

TEST(ShellApiTest, GetSpecialFolderPath) {
  const boost::filesystem::path fallback = "C:\\Fallback";
  // CSIDL_PROFILE is usually available
  const boost::filesystem::path path = shellapi::get_special_folder_path(CSIDL_PROFILE, fallback);
  EXPECT_FALSE(path.empty());
  EXPECT_NE(path, fallback);
  EXPECT_TRUE(boost::filesystem::exists(path));

  // Test with an invalid CSIDL
  const boost::filesystem::path invalid_path = shellapi::get_special_folder_path(-1, fallback);
  EXPECT_EQ(invalid_path, fallback);
}

TEST(ShellApiTest, GetTempPath) {
  const boost::filesystem::path path = shellapi::get_temp_path();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(boost::filesystem::exists(path));
}

#endif
