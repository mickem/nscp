#ifdef WIN32
// clang-format off
#include <win/windows.hpp>
#include <Shlobj.h>
#include <shellapi.h>
// clang-format on
#include <boost/filesystem/path.hpp>

namespace shellapi {
boost::filesystem::path get_module_file_name();
boost::filesystem::path get_special_folder_path(int csidl, boost::filesystem::path fallback);
boost::filesystem::path get_temp_path();
}  // namespace shellapi
#endif
