// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "path_manager.hpp"

#include <config.h>

#include <parsers/expression/expression.hpp>
#include <str/utf8.hpp>

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
  // Note: init_settings() pushes this same value into the settings core via
  // set_base(provider->expand_path("${base-path}")) right after construction.
  // No need to duplicate that call here - keeps path_manager free of any
  // settings_manager dependency.
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
  // Dynamic lookups that need member state or runtime OS calls.
  if (key == "base-path" || key == "exe-path") return getBasePath().string();
  if (key == "temp") return getTempPath().string();
#ifdef WIN32
  if (key == "shared-path") return getBasePath().string();
  if (key == "data-path" || key == "appdata") return shellapi::get_special_folder_path(CSIDL_APPDATA, getBasePath()).string();
  if (key == "common-appdata") return shellapi::get_special_folder_path(CSIDL_COMMON_APPDATA, getBasePath()).string();
#endif

  // Static defaults baked in by CMake via config.h. Most are templated on
  // ${shared-path} or ${certificate-path}; expand_path resolves the chain.
  // Note on ca-path: on Windows the service exports the system ROOT store to
  // this file at boot (see windows_ca_store); on Linux this is the de-facto
  // Debian/Ubuntu location, overridable via boot.ini's [paths] section
  // (or the --path-override CLI flag) for other distros.
  static const std::map<std::string, std::string> defaults = {
      {"certificate-path", CERT_FOLDER},
      {"module-path", MODULE_FOLDER},
      {"web-path", WEB_FOLDER},
      {"scripts", SCRIPTS_FOLDER},
      {"log-path", LOG_FOLDER},
      {CACHE_FOLDER_KEY, DEFAULT_CACHE_PATH},
      {CRASH_ARCHIVE_FOLDER_KEY, "${shared-path}/crash-dumps"},
#ifdef WIN32
      {"ca-path", "${certificate-path}/windows-ca.pem"},
#else
      // ca-path stays a literal absolute path: the system CA bundle belongs to
      // the distro, lives at /etc/ssl/... regardless of our install prefix, and
      // must NOT track ${etc}/NSCP_SYSCONFDIR (a --prefix=/usr/local build still
      // reads /etc/ssl/certs/..., not /usr/local/etc/ssl/...).
      {"ca-path", "/etc/ssl/certs/ca-certificates.crt"},
      {"shared-path", UNIX_SHARED_PATH_FOLDER},
      {"data-path", UNIX_DATA_PATH_FOLDER},
      // ${etc} tracks this build's config root (NSCP_SYSCONFDIR) so user
      // ${etc}/... includes follow the prefix.
      {"etc", ETC_FOLDER},
      // boot.ini's default location, expressed as a token off ${etc} so it
      // both tracks the prefix and stays CLI-overridable
      // (--path-override boot-conf=/path/to/boot.ini). Expands cleanly with no
      // self-reference; the kMaxExpandDepth guard catches a misconfigured cycle.
      {"boot-conf", "${etc}/nsclient/boot.ini"},
#endif
  };

  const auto it = defaults.find(key);
  if (it != defaults.end()) return it->second;
  return getBasePath().string();
}

void nsclient::core::path_manager::set_overrides(paths_type overrides) { overrides_ = std::move(overrides); }

void nsclient::core::path_manager::add_overrides(paths_type overrides) {
  for (auto &kv : overrides) {
    overrides_[kv.first] = std::move(kv.second);
  }
}

void nsclient::core::path_manager::set_cli_overrides(paths_type overrides) { cli_overrides_ = std::move(overrides); }

std::string nsclient::core::path_manager::getFolder(const std::string &key) {
  // Precedence: CLI --path-override > boot.ini [paths] > compile-time defaults.
  const auto cli = cli_overrides_.find(key);
  if (cli != cli_overrides_.end()) return cli->second;
  const auto it = overrides_.find(key);
  if (it != overrides_.end()) return it->second;
  return get_path_for_key(key);
}

std::string nsclient::core::path_manager::expand_path(std::string file) { return expand_path_impl(std::move(file), 0); }

std::string nsclient::core::path_manager::expand_path_impl(std::string file, const int depth) {
  // Cycle guard: a settings cycle ("${a}" -> "${b}" -> "${a}") used to
  // recurse without bound and either stack-overflow the service (uncatchable
  // on Windows) or burn the whole stack before the catch(...) below kicked in
  // on POSIX. Bail at a fixed depth and log loudly so an operator can
  // identify the cycle from the surfaced error message.
  if (depth > kMaxExpandDepth) {
    LOG_ERROR_CORE("Refusing to expand path beyond " + std::to_string(kMaxExpandDepth) +
                   " levels (cycle in boot.ini [paths]?): " + utf8::cvt<std::string>(file));
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
