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

namespace {
// True when `dir` actually holds the shipped DH parameters, which is what
// decides between the ${nrpe-dh} candidates. Errors count as "no": an
// unreadable or missing folder is not one we want to hand to OpenSSL.
bool holds_nrpe_dh_params(const std::string &dir) {
  if (dir.empty()) return false;
  boost::system::error_code ec;
  const boost::filesystem::path path(dir);
  if (!boost::filesystem::is_directory(path, ec) || ec) return false;
  boost::filesystem::directory_iterator it(path, ec), end;
  for (; !ec && it != end; it.increment(ec)) {
    if (nscp::paths::is_nrpe_dh_file(it->path().filename().string())) return true;
  }
  return false;
}
}  // namespace

std::string nsclient::core::path_manager::resolve_nrpe_dh(const int depth) {
  // The DH parameters are shipped package content, so on Windows the installer
  // leaves them beside the executable while ${certificate-path} moves to
  // %ProgramData% under the modern layout - one token cannot name both. Try
  // the modern location first so an operator who drops their own parameters in
  // with the rest of the writable state wins, then fall back to where the
  // installer put them.
  //
  // When neither candidate has them we still answer with the last one rather
  // than an empty string: the file is missing either way, and naming a real
  // folder makes OpenSSL's error message point somewhere an operator can act
  // on. Note this is a lookup on every resolution by design - the answer
  // changes when an upgrade or a migration moves the files, and the option is
  // expanded at module load, not per request.
  std::string last;
  for (const char *const *candidate = nscp::paths::nrpe_dh_candidates(); *candidate != nullptr; ++candidate) {
    last = expand_path_impl(resolve_folder(*candidate, depth + 1), depth + 1);
    if (holds_nrpe_dh_params(last)) return last;
  }
  return last;
}

std::string nsclient::core::path_manager::get_path_for_key(const std::string &key, const int depth) {
  // Dynamic lookups that need member state or runtime OS calls.
  if (key == "base-path" || key == "exe-path") return getBasePath().string();
  if (key == "temp") return getTempPath().string();
  if (key == "nrpe-dh") return resolve_nrpe_dh(depth);
#ifdef WIN32
  if (key == "data-path" || key == "appdata") return shellapi::get_special_folder_path(CSIDL_APPDATA, getBasePath()).string();
  if (key == "common-appdata") return shellapi::get_special_folder_path(CSIDL_COMMON_APPDATA, getBasePath()).string();
#endif

  // Static defaults, shared with the standalone clients so the two cannot
  // disagree about where anything is - see include/nscp/path_defaults.hpp.
  // On Windows this is also what moves ${shared-path} when the operator has
  // opted into the modern layout; an empty answer means "no static default",
  // which for shared-path is the legacy answer of "next to the executable".
  const std::string shared_default = nscp::paths::default_for(key, layout_);
  if (!shared_default.empty()) return shared_default;
#ifdef WIN32
  if (key == "shared-path") return getBasePath().string();
#endif

  // Anything we have no answer for resolves to the executable's directory,
  // which is the historical behaviour and keeps a typo in a settings file from
  // expanding to an empty (and therefore root-relative) path.
  return getBasePath().string();
}

void nsclient::core::path_manager::set_layout(const nscp::paths::layout value) { layout_ = value; }

void nsclient::core::path_manager::set_overrides(paths_type overrides) { overrides_ = std::move(overrides); }

void nsclient::core::path_manager::add_overrides(paths_type overrides) {
  for (auto &kv : overrides) {
    overrides_[kv.first] = std::move(kv.second);
  }
}

void nsclient::core::path_manager::set_cli_overrides(paths_type overrides) { cli_overrides_ = std::move(overrides); }

std::string nsclient::core::path_manager::getFolder(const std::string &key) { return resolve_folder(key, 0); }

std::string nsclient::core::path_manager::resolve_folder(const std::string &key, const int depth) {
  // Precedence: CLI --path-override > boot.ini [paths] > compile-time defaults.
  const auto cli = cli_overrides_.find(key);
  if (cli != cli_overrides_.end()) return cli->second;
  const auto it = overrides_.find(key);
  if (it != overrides_.end()) return it->second;
  return get_path_for_key(key, depth);
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
        ret += expand_path_impl(resolve_folder(e.name, depth + 1), depth + 1);
    }
    return ret;
  } catch (...) {
    LOG_ERROR_CORE("Failed to expand path: " + utf8::cvt<std::string>(file));
    return "";
  }
}
