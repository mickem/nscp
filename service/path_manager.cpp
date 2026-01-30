#include "path_manager.hpp"

#include <config.h>

#include <parsers/expression/expression.hpp>
#include <utf8.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"

#ifdef WIN32
// clang-format off
#include <win/windows.hpp>
#include <Shlobj.h>
#include <shellapi.h>
// clang-format on
#endif

#include <boost/filesystem.hpp>

nsclient::core::path_manager::path_manager(const logging::log_client_accessor &log_instance_) : log_instance_(log_instance_) {}

#ifdef WIN32
boost::filesystem::path get_selfpath() {
  wchar_t buff[4096];
  if (GetModuleFileName(nullptr, buff, sizeof(buff) - 1)) {
    const boost::filesystem::path p = std::wstring(buff);
    return p.parent_path();
  }
  return boost::filesystem::initial_path();
}
#else
boost::filesystem::path get_selfpath() {
  char buff[1024];
  ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
  if (len != -1) {
    buff[len] = '\0';
    boost::filesystem::path p = std::string(buff);
    return p.parent_path();
  }
  return boost::filesystem::initial_path();
}
#endif
boost::filesystem::path nsclient::core::path_manager::getBasePath() {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get mutex.");
    return boost::filesystem::path("/");
  }
  if (!basePath.empty()) {
    return basePath;
  }
  basePath = get_selfpath();
  try {
    settings_manager::get_core()->set_base(basePath);
  } catch (settings::settings_exception &e) {
    LOG_ERROR_CORE_STD("Failed to set settings file: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Failed to set settings file");
  }
  return basePath;
}

#ifdef WIN32
typedef DWORD(WINAPI *PFGetTempPath)(__in DWORD nBufferLength, __out LPTSTR lpBuffer);
#endif
boost::filesystem::path nsclient::core::path_manager::getTempPath() {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get mutex.");
    return "";
  }
  if (!tempPath.empty()) return tempPath;
#ifdef WIN32
  HMODULE hKernel = ::LoadLibrary(L"kernel32");
  if (hKernel) {
    // Find PSAPI functions
    auto FGetTempPath = reinterpret_cast<PFGetTempPath>(::GetProcAddress(hKernel, "GetTempPathW"));
    if (FGetTempPath) {
      constexpr unsigned int buf_len = 4096;
      auto *buffer = new wchar_t[buf_len + 1];
      if (FGetTempPath(buf_len, buffer)) {
        tempPath = buffer;
      }
      delete[] buffer;
    }
  }
#else
  tempPath = "/tmp";
#endif
  return tempPath;
}
std::string nsclient::core::path_manager::get_app_data_path() {
#ifdef WIN32
  wchar_t buf[MAX_PATH + 1];
  if (SHGetSpecialFolderPath(nullptr, buf, CSIDL_APPDATA, FALSE)) {
    return utf8::cvt<std::string>(buf) + "\\nsclient";
  }
  return getBasePath().string();
#else
  return "/var/lib/nsclient";
#endif
}

std::string nsclient::core::path_manager::getFolder(const std::string &key) {
  {
    const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (lock.owns_lock()) {
      const auto p = paths_cache_.find(key);
      if (p != paths_cache_.end()) {
        return p->second;
      }
    }
  }

  const auto base_path = getBasePath();
  std::string default_value = base_path.string();
  if (key == "certificate-path") {
    default_value = CERT_FOLDER;
  } else if (key == "module-path") {
    default_value = MODULE_FOLDER;
  } else if (key == "web-path") {
    default_value = WEB_FOLDER;
  } else if (key == "scripts") {
    default_value = SCRIPTS_FOLDER;
  } else if (key == "log-path") {
    default_value = LOG_FOLDER;
  } else if (key == CACHE_FOLDER_KEY) {
    default_value = DEFAULT_CACHE_PATH;
  } else if (key == CRASH_ARCHIVE_FOLDER_KEY) {
    default_value = "${shared-path}/crash-dumps";
  } else if (key == "base-path") {
    // Use default;
  } else if (key == "temp") {
    default_value = getTempPath().string();
  } else if (key == "shared-path") {
#ifdef WIN32
    // Use default;
#else
    default_value = UNIX_SHARED_PATH_FOLDER;
#endif
  } else if (key == "base-path" || key == "exe-path") {
    // Use default;
  } else if (key == "data-path") {
    default_value = get_app_data_path();
#ifdef WIN32
  } else if (key == "common-appdata") {
    wchar_t buf[MAX_PATH + 1];
    if (SHGetSpecialFolderPath(nullptr, buf, CSIDL_COMMON_APPDATA, FALSE))
      default_value = utf8::cvt<std::string>(buf);
    else
      default_value = getBasePath().string();
  } else if (key == "appdata") {
    wchar_t buf[MAX_PATH + 1];
    if (SHGetSpecialFolderPath(nullptr, buf, CSIDL_APPDATA, FALSE))
      default_value = utf8::cvt<std::string>(buf);
    else
      default_value = getBasePath().string();
  }
#else
  } else if (key == "etc") {
    default_value = "/etc";
  }
#endif
  try {
    if (settings_manager::get_core()->is_ready()) {
      std::string path = settings_manager::get_settings()->get_string(CONFIG_PATHS, key, default_value);
      settings_manager::get_core()->register_key(0xffff, CONFIG_PATHS, key, "file", "Path for " + key, "", default_value, false, false);
      paths_cache_[key] = path;
      return path;
    } else {
      LOG_DEBUG_CORE("Settings not ready so we cant lookup: " + key);
    }
  } catch (const settings::settings_exception &e) {
    // TODO: Maybe this should be fixed!
    paths_cache_[key] = default_value;
  }
  return default_value;
}

std::string nsclient::core::path_manager::expand_path(std::string file) {
  try {
    if (file.empty()) return file;
    parsers::simple_expression::result_type expr;
    parsers::simple_expression::parse(file, expr);

    std::string ret;
    for (const parsers::simple_expression::entry &e : expr) {
      if (!e.is_variable)
        ret += e.name;
      else
        ret += expand_path(getFolder(e.name));
    }
    return ret;
  } catch (...) {
    LOG_ERROR_CORE("Failed to expand path: " + utf8::cvt<std::string>(file));
    return "";
  }
}
