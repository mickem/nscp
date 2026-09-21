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

#include <utility>
#include <vector>

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
  throw path_expansion_error("Unknown path token ${" + key +
                             "}: no such path is configured. Check the spelling, or define it in the [paths] section of "
                             "boot.ini.");
}

void nsclient::core::path_manager::set_layout(const nscp::paths::layout value) { layout_ = value; }

namespace {
// ${boot-conf} names boot.ini itself, so the bootstrap consumes it *before* the
// deferred pass below can run. "Ask again later" therefore never comes for it
// and it has to be judged on the spot, with whatever resolves at that point.
bool is_consumed_before_boot(const std::string &key) { return key == "boot-conf"; }
}  // namespace

std::string nsclient::core::path_manager::why_unusable(const std::string &value, bool &unresolved) {
  unresolved = false;
  try {
    const std::string resolved = expand_path_impl(value, 0);
    if (resolved.empty()) return "it expands to nothing";
    if (!nscp::paths::names_a_root(resolved))
      // The same predicate resolve_path() uses to decide whether a value
      // already names a location. Deliberately shared: the alternative was
      // is_absolute() here and names_a_root() there, which meant a Windows
      // root-relative `\logs` was accepted when written in a setting and
      // rejected when written as the override for that same folder.
      return "it does not name an absolute location (it resolves to '" + resolved + "')";
    return "";
  } catch (const path_expansion_error &e) {
    // Not necessarily final: an override may name a token boot.ini has yet to
    // define. The caller decides whether this key can wait for that.
    unresolved = true;
    return e.what();
  }
}

void nsclient::core::path_manager::drop_unusable_overrides(paths_type &map, const char *source, const bool defer_unresolved) {
  // Judge a whole pass against the map as it stands, then remove, then look
  // again - rather than erasing as we walk. Erasing in place made the outcome
  // depend on std::map's key order: with `logs = relative` and
  // `mine = ${logs}/x` both installed, `mine` was judged against a still
  // present `logs` and dropped when it happened to sort first, and against the
  // built-in default and kept when it sorted last. Same configuration, two
  // answers. Deciding everything before removing anything makes the verdict a
  // property of the configuration, and iterating carries a verdict that only
  // changed because of a removal through to the end.
  struct rejection {
    std::string key;
    std::string value;
    std::string why;
  };
  for (;;) {
    std::vector<rejection> doomed;
    for (const auto &kv : map) {
      bool unresolved = false;
      const std::string why = why_unusable(kv.second, unresolved);
      if (why.empty()) continue;
      // Not usable *yet*, and this key can wait: leave it for the pass that
      // runs once boot.ini has been read. It throws rather than mis-resolving
      // in the meantime, so nothing silently uses the wrong folder.
      if (unresolved && defer_unresolved && !is_consumed_before_boot(kv.first)) continue;
      doomed.push_back(rejection{kv.first, kv.second, why});
    }
    if (doomed.empty()) return;
    for (const rejection &bad : doomed) {
      // Dropped rather than kept, so the compiled-in default applies: that is a
      // defined absolute location, where a relative override is read and written
      // relative to the service's working directory - System32 for a Windows
      // service, "/" under a bare init, the package directory under the shipped
      // systemd unit. An operator cannot predict which, so we do not guess for
      // them; we say so and use the default.
      LOG_ERROR_CORE("Ignoring the " + std::string(source) + " entry '" + bad.key + " = " + bad.value + "': " + bad.why +
                     ". A path token has to resolve to an absolute path; using the built-in default for ${" + bad.key + "} instead." +
                     (is_consumed_before_boot(bad.key) ? " ${" + bad.key +
                                                             "} is resolved to find boot.ini itself, before anything that could make sense of a "
                                                             "relative or later-defined value has been read, so it cannot wait."
                                                       : ""));
      map.erase(bad.key);
    }
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
  cli_overrides_ = std::move(overrides);

  // Judged here and not only in validate_overrides(), because these are in
  // force for the whole of init_settings(): boot.ini is opened through
  // ${boot-conf}, the shared folder is created and locked down through
  // ${shared-path}, and the trust store is written through
  // ${certificate-path}, all before the later pass runs. An override that is
  // already definitively unusable - it resolves, and what it resolves to is
  // not a location - therefore has to go before any of that happens, or the
  // configuration is read out of one tree while scripts, certificates and the
  // settings cache are written into another.
  //
  // Only the verdict is taken from this early pass, never the resolved value,
  // which is what makes it safe to run before boot.ini's [layout] has been
  // applied: every layout answers ${shared-path} with an absolute path, so
  // which one is in force cannot change "does this name a location?".
  //
  // What genuinely cannot be judged yet is deferred rather than guessed at. An
  // override is explicitly allowed to be built out of a [paths] entry boot.ini
  // has not been read for yet ("scripts = ${mine}/bin"), and such a value only
  // throws while that is so - it never quietly resolves to the wrong folder -
  // so waiting costs nothing. ${boot-conf} is the one key that cannot wait,
  // and drop_unusable_overrides knows it by name.
  drop_unusable_overrides(cli_overrides_, "--path-override", /*defer_unresolved=*/true);
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
  // on POSIX. Bail at a fixed depth.
  //
  // Raised rather than answered with an empty string, which is how this first
  // bailed out. Only the innermost frame saw the empty answer: every frame
  // above it appended its own suffix to it, so a self-referential override
  // (`boot-conf=${boot-conf}/boot.ini`) came back as `/boot.ini/boot.ini/...`
  // - a value that names a root, which is exactly what the override check
  // asks for, so the cycle was validated and kept. A cycle is a configuration
  // error of the same kind as an unknown token and is reported the same way.
  if (depth > kMaxExpandDepth) {
    throw path_expansion_error("Refusing to expand path beyond " + std::to_string(kMaxExpandDepth) + " levels: '" + utf8::cvt<std::string>(file) +
                               "' is part of a cycle. Check the [paths] section of boot.ini and any --path-override for a token that names itself.");
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
