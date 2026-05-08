#include "path_manager.hpp"

#include <config.h>

#include <parsers/expression/expression.hpp>
#include <str/utf8.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"

#ifdef WIN32
#include <win/shellapi.hpp>
#endif

#include <boost/filesystem.hpp>

nsclient::core::path_manager::path_manager(const logging::log_client_accessor &log_instance_) : log_instance_(log_instance_) {}

boost::filesystem::path get_exe_path() {
#ifdef WIN32
  return shellapi::get_module_file_name();
#else
  char buff[1024];
  ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
  if (len != -1) {
    buff[len] = '\0';
    boost::filesystem::path p = std::string(buff);
    return p.parent_path();
  }
  return boost::filesystem::initial_path();
#endif
}
boost::filesystem::path nsclient::core::path_manager::getBasePath() {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) {
    LOG_ERROR_CORE("FATAL ERROR: Could not get mutex.");
    return boost::filesystem::path("/");
  }
  if (!basePath.empty()) {
    return basePath;
  }
  basePath = get_exe_path();
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
  tempPath = shellapi::get_temp_path();
#else
  tempPath = "/tmp";
#endif
  return tempPath;
}
boost::filesystem::path nsclient::core::path_manager::get_app_data_path() {
#ifdef WIN32
  return shellapi::get_special_folder_path(CSIDL_APPDATA, getBasePath());
#else
  return UNIX_DATA_PATH_FOLDER;
#endif
}

std::string nsclient::core::path_manager::get_path_for_key(const std::string &key) {
  if (key == "certificate-path") {
    return CERT_FOLDER;
  }
  if (key == "ca-path") {
    // Default trusted-CA bundle. On Windows the service exports the system
    // ROOT store to this file at boot (see windows_ca_store). On Linux we
    // point at the de-facto Debian/Ubuntu location; operators on other
    // distros (or who manage their own bundle) can override this in
    // [/paths] without touching every module.
#ifdef WIN32
    return "${certificate-path}/windows-ca.pem";
#else
    return "/etc/ssl/certs/ca-certificates.crt";
#endif
  }
  if (key == "module-path") {
    return MODULE_FOLDER;
  }
  if (key == "web-path") {
    return WEB_FOLDER;
  }
  if (key == "scripts") {
    return SCRIPTS_FOLDER;
  }
  if (key == "log-path") {
    return LOG_FOLDER;
  }
  if (key == CACHE_FOLDER_KEY) {
    return DEFAULT_CACHE_PATH;
  }
  if (key == CRASH_ARCHIVE_FOLDER_KEY) {
    return "${shared-path}/crash-dumps";
  }
  if (key == "base-path") {
    return getBasePath().string();
  }
  if (key == "temp") {
    return getTempPath().string();
  }
  if (key == "shared-path") {
#ifdef WIN32
    return getBasePath().string();
#else
    return UNIX_SHARED_PATH_FOLDER;
#endif
  }
  if (key == "base-path" || key == "exe-path") {
    return getBasePath().string();
  }
  if (key == "data-path") {
#ifdef WIN32
    return shellapi::get_special_folder_path(CSIDL_APPDATA, getBasePath()).string();
#else
    return UNIX_DATA_PATH_FOLDER;
#endif
#ifdef WIN32
  }
  if (key == "common-appdata") {
    return shellapi::get_special_folder_path(CSIDL_COMMON_APPDATA, getBasePath()).string();
  }
  if (key == "appdata") {
    return shellapi::get_special_folder_path(CSIDL_APPDATA, getBasePath()).string();
#else
  } else if (key == "etc") {
    return "/etc";
#endif
  }
  return getBasePath().string();
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

  const auto default_value = get_path_for_key(key);
  try {
    if (settings_manager::get_core()->is_ready()) {
      std::string path = settings_manager::get_settings()->get_string(CONFIG_PATHS, key, default_value);
      settings_manager::get_core()->register_key(0xffff, CONFIG_PATHS, key, "file", "Path for " + key, "", default_value, false, false);
      paths_cache_[key] = path;
      return path;
    } else {
      LOG_DEBUG_CORE("Settings not ready so we cant lookup: " + key);
    }
  } catch (const settings::settings_exception &) {
    // TODO: Maybe this should be fixed!
    paths_cache_[key] = default_value;
  }
  return default_value;
}

std::string nsclient::core::path_manager::expand_path(std::string file) { return expand_path_impl(std::move(file), 0); }

std::string nsclient::core::path_manager::expand_path_impl(std::string file, const int depth) {
  // Cycle guard: a settings cycle ("${a}" -> "${b}" -> "${a}") used to
  // recurse without bound and either stack-overflow the service (uncatchable
  // on Windows) or burn the whole stack before the catch(...) below kicked in
  // on POSIX. Bail at a fixed depth and log loudly so an operator can
  // identify the cycle from the surfaced error message.
  if (depth > kMaxExpandDepth) {
    LOG_ERROR_CORE("Refusing to expand path beyond " + std::to_string(kMaxExpandDepth) + " levels (cycle in /paths?): " + utf8::cvt<std::string>(file));
    return "";
  }
  try {
    if (file.empty()) return file;
    parsers::simple_expression::result_type expr;
    parsers::simple_expression::parse(file, expr);

    std::string ret;
    for (const parsers::simple_expression::entry &e : expr) {
      if (!e.is_variable)
        ret += e.name;
      else
        ret += expand_path_impl(getFolder(e.name), depth + 1);
    }
    return ret;
  } catch (...) {
    LOG_ERROR_CORE("Failed to expand path: " + utf8::cvt<std::string>(file));
    return "";
  }
}
