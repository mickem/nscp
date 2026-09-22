// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "path_manager.hpp"

#include <config.h>

#include <nscp/path_rooting.hpp>
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

  // A token we have no answer for is a typo, and saying so is the whole point.
  // This used to resolve to the executable's directory, which meant
  // `${scripst}/x.bat` was not an error but a real path under the install
  // folder - so the file went somewhere nobody was looking and nothing
  // complained (#458). An operator cannot fix what is never reported.
  throw path_expansion_error("Unknown path token ${" + key + "}: no such path is configured. Check the spelling, or define it in the [paths] section of "
                                                            "boot.ini.");
}

void nsclient::core::path_manager::set_layout(const nscp::paths::layout value) { layout_ = value; }

void nsclient::core::path_manager::drop_unusable_overrides(paths_type &map, const char *source) {
  // Run after the map is installed, not before, because an override may be
  // written in terms of other tokens ("scripts = ${shared-path}/mine") and one
  // override may reference another. Expanding here therefore sees the same
  // answers the rest of the service will.
  for (auto it = map.begin(); it != map.end();) {
    std::string why;
    try {
      const std::string resolved = expand_path_impl(it->second, 0);
      if (resolved.empty())
        why = "it expands to nothing";
      else if (!nscp::paths::names_a_root(resolved))
        // The same predicate resolve_path() uses to decide whether a value
        // already names a location. Deliberately shared: the alternative was
        // is_absolute() here and names_a_root() there, which meant a Windows
        // root-relative `\\logs` was accepted when written in a setting and
        // rejected when written as the override for that same folder.
        // "names a location of its own", not "is absolute": the gate is
        // names_a_root(), which by design also accepts a Windows drive-relative
        // C:sub and a root-relative \\logs. Say what is enforced rather than
        // promising something stricter.
        why = "it does not name a location of its own (it resolves to '" + resolved + "')";
    } catch (const path_expansion_error &e) {
      why = e.what();
    }
    if (why.empty()) {
      ++it;
      continue;
    }
    // Dropped rather than kept, so the compiled-in default applies: that is a
    // defined absolute location, where a relative override is read and written
    // relative to the service's working directory - System32 for a Windows
    // service, "/" under a bare init, the package directory under the shipped
    // systemd unit. An operator cannot predict which, so we do not guess for
    // them; we say so and use the default.
    LOG_ERROR_CORE("Ignoring the " + std::string(source) + " entry '" + it->first + " = " + it->second + "': " + why +
                   ". A path token has to resolve to an absolute path; using the built-in default for ${" + it->first + "} instead.");
    it = map.erase(it);
  }
}

void nsclient::core::path_manager::set_overrides(paths_type overrides) {
  overrides_ = std::move(overrides);
  drop_unusable_overrides(overrides_, "boot.ini [paths]");
}

void nsclient::core::path_manager::add_overrides(paths_type overrides) {
  for (auto &kv : overrides) {
    overrides_[kv.first] = std::move(kv.second);
  }
  drop_unusable_overrides(overrides_, "boot.ini [paths]");
}

void nsclient::core::path_manager::set_cli_overrides(paths_type overrides) {
  // Installed without validating, unlike the boot.ini layer. This runs before
  // init_settings(), so boot.ini has been read neither for [layout] - which on
  // Windows decides what ${shared-path} means - nor for [paths], whose entries
  // an operator is explicitly allowed to build a CLI override out of. Judging
  // an override against a half-built picture would reject perfectly good ones
  // and resolve the rest against the wrong layout. validate_overrides() does it
  // once the picture is complete.
  cli_overrides_ = std::move(overrides);

  // ...with one exception, because one key cannot wait. ${boot-conf} names
  // boot.ini itself, so init_settings() consumes it *before* the picture is
  // complete and validate_overrides() can run. A relative value there would be
  // used once, to find and read a file relative to the working directory, and
  // then dropped - leaving the bootstrap having read one file while every later
  // ${boot-conf} expansion names another. It also needs no deferral: it is
  // resolved before [paths] exists, so it cannot legitimately be built out of a
  // token boot.ini defines.
  //
  // It is the *resolved* value that has to name a root, not the spelling. The
  // built-in default is itself written with a token (${exe-path}/boot.ini on
  // Windows, ${etc}/nsclient/boot.ini on unix), so judging the raw string would
  // reject the very form the CLI documents - and would contradict the rule the
  // other overrides follow, that an override may be built out of tokens as long
  // as what it comes to is absolute. The tokens that can legitimately appear
  // here are the compile-time ones, which resolve without boot.ini; one naming
  // a [paths] entry boot.ini has yet to define fails to expand, and that is a
  // rejection too. expand_path_impl's depth guard covers a self-referential
  // value.
  const paths_type::const_iterator boot = cli_overrides_.find("boot-conf");
  if (boot != cli_overrides_.end()) {
    std::string resolved;
    std::string why;
    try {
      resolved = expand_path(boot->second);
      if (!nscp::paths::names_a_root(resolved)) why = "it resolves to '" + resolved + "', which names no location of its own";
    } catch (const std::exception &e) {
      why = std::string("it could not be resolved: ") + e.what();
    }
    if (!why.empty()) {
      get_logger()->error("core", __FILE__, __LINE__,
                          "Ignoring --path-override boot-conf=" + boot->second + ": " + why +
                              ". It is used to find boot.ini before anything that could make sense of a relative one has been read. Using the default.");
      cli_overrides_.erase("boot-conf");
    }
  }
}

void nsclient::core::path_manager::validate_overrides() {
  // Called once the bootstrap has applied boot.ini's [layout] and [paths], so
  // every token an override may legitimately name now resolves. Idempotent: an
  // override that already passed simply passes again, which is what lets the
  // boot.ini layer be checked when it is installed and re-checked here without
  // the two disagreeing.
  drop_unusable_overrides(cli_overrides_, "--path-override");
  drop_unusable_overrides(overrides_, "boot.ini [paths]");
}

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

std::string nsclient::core::path_manager::resolve_path(std::string file, const std::string &default_root) {
  return nscp::paths::root_path(std::move(file), default_root, [this](std::string value) { return expand_path_impl(std::move(value), 0); });
}

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
    // `none` names no file at all (log file off, ca -> the library's own trust
    // store). It is a sentinel rather than a path, so it passes through
    // untouched - and, because it never reaches the joining logic, it cannot
    // be turned into a file literally called `none`.
    if (nscp::paths::is_no_path(file)) return file;
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
  } catch (const path_expansion_error &) {
    // An unknown token is a reportable configuration error, not a failure to
    // be flattened into an empty string: the callers that expand
    // operator-supplied paths catch this and name what they were configuring.
    throw;
  } catch (...) {
    LOG_ERROR_CORE("Failed to expand path: " + utf8::cvt<std::string>(file));
    return "";
  }
}
