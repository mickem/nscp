// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <config.h>

#include <map>
#include <string>

// The compiled-in defaults behind the ${...} path tokens, in one place.
//
// Three programs resolve these tokens: the service (nsclient::core::path_manager)
// and the two standalone clients (check_nrpe, check_nscp), which cannot use
// path_manager because they never construct a core. They used to carry
// independent copies of the table, which is exactly the kind of duplication
// that survives right up until one of the values changes - and the modern
// layout below changes the most load-bearing one, ${shared-path}.
//
// Only the *static* defaults live here. Tokens whose value is a runtime lookup
// (base-path, exe-path, temp, common-appdata, and appdata on Windows) stay with
// each caller, which knows its own executable and its own fallbacks.
//
// This header is deliberately dependency-free beyond config.h so the clients
// can use it without linking anything new.
namespace nscp {
namespace paths {

// Which on-disk layout this installation uses.
//
// `legacy` keeps everything beside the executable, which on Windows means
// Program Files: writable only by administrators, and read-only for the
// service in every way that matters. `modern` moves the writable state to
// %ProgramData%\NSClient++.
//
// Opt-in, and per installation: an operator selects it in boot.ini
// (`[layout] mode = modern`), so both layouts can exist in the field while the
// migration is rolled out. Unix has no such switch - its layout is decided by
// the package prefix and is already correct.
enum class layout { legacy, modern };

// The folder name used under %ProgramData%.
inline const char *shared_folder_name() { return "NSClient++"; }

// Parse the boot.ini value. Unknown or empty text is `legacy`: an operator who
// mistypes the mode gets the layout they already had, not a half-migrated one.
// `v2` is accepted as a synonym for `modern`.
inline layout parse_layout(const std::string &value) {
  if (value == "modern" || value == "v2") return layout::modern;
  return layout::legacy;
}

// True when `value` names a layout we understand, so a caller can tell "the
// operator asked for legacy" from "the operator asked for something we do not
// recognise" and warn about the latter.
inline bool is_known_layout(const std::string &value) { return value.empty() || value == "legacy" || value == "v1" || value == "modern" || value == "v2"; }

inline const char *layout_name(const layout value) { return value == layout::modern ? "modern" : "legacy"; }

// True for the shipped Diffie-Hellman parameter files (nrpe_dh_512.pem,
// nrpe_dh_2048.pem). They are package content rather than machine state, so
// two very different pieces of code have to agree on what "the DH files" are:
// the layout migration, which leaves them behind, and the ${nrpe-dh} lookup,
// which then has to find them where they were left. Prefix-only on purpose -
// the migration has always treated anything named nrpe_dh_* as shipped, and
// narrowing that here would start moving files it used to keep.
inline bool is_nrpe_dh_file(const std::string &name) { return name.rfind("nrpe_dh_", 0) == 0; }

// The tokens ${nrpe-dh} chooses between, in preference order. Resolving the
// alias needs the filesystem, so it happens in the caller (path_manager);
// this header only names the candidates so there is one list, not two.
inline const char *const *nrpe_dh_candidates() {
  static const char *const candidates[] = {"modern-nrpe-dh", "legacy-nrpe-dh", nullptr};
  return candidates;
}

// The static default for `key`, or an empty string when the key has no static
// default (the caller then resolves it itself, or falls back to the base path).
//
// Values may themselves contain tokens - `${shared-path}/security` and so on -
// so the caller's expander has to resolve the result recursively.
inline std::string default_for(const std::string &key, const layout current) {
#ifdef WIN32
  // The one token the layout actually moves. Everything else is expressed
  // relative to it, so they all follow without needing a second opinion here.
  //
  // Returned as a token expression rather than an absolute path because every
  // caller already resolves ${common-appdata}; this keeps the header free of
  // platform lookups.
  if (key == "shared-path") {
    if (current == layout::modern) return std::string("${common-appdata}/") + shared_folder_name();
    return std::string();  // legacy: the caller's own executable directory
  }
#else
  static_cast<void>(current);
#endif

  static const std::map<std::string, std::string> defaults = {
      {"certificate-path", CERT_FOLDER},
      {"modern-nrpe-dh", MODERN_NRPE_DH_FOLDER},
      {"legacy-nrpe-dh", LEGACY_NRPE_DH_FOLDER},
      // ${nrpe-dh} is normally a filesystem lookup over the two candidates
      // above, which only the service can do. This is the answer for callers
      // that expand tokens without a path_manager (the standalone clients):
      // the modern location, i.e. the same thing ${certificate-path} says.
      {"nrpe-dh", MODERN_NRPE_DH_FOLDER},
      {"module-path", MODULE_FOLDER},
      {"web-path", WEB_FOLDER},
      {"scripts", SCRIPTS_FOLDER},
      {"log-path", LOG_FOLDER},
      {"ca-path", CA_PATH},
      {CACHE_FOLDER_KEY, DEFAULT_CACHE_PATH},
      {CRASH_ARCHIVE_FOLDER_KEY, CRASH_ARCHIVE_FOLDER},
      // Everything the fleet sync owns lives here: the rendered fleet.ini that
      // nsclient.ini includes, the staged scripts and the bundle cache. The
      // default is per-platform (CONFIG_FLEET_FOLDER) because it has to be
      // writable by the account the service runs as, which on unix rules out
      // the package directory ${shared-path} points at.
      {FLEET_FOLDER_KEY, FLEET_FOLDER},
#ifndef WIN32
      {"shared-path", UNIX_SHARED_PATH_FOLDER},
      {"data-path", UNIX_DATA_PATH_FOLDER},
      // ${etc} tracks this build's config root (NSCP_SYSCONFDIR) so user
      // ${etc}/... includes follow the prefix.
      {"etc", ETC_FOLDER},
      // boot.ini's default location, expressed as a token off ${etc} so it
      // both tracks the prefix and stays CLI-overridable
      // (--path-override boot-conf=/path/to/boot.ini). Expands cleanly with no
      // self-reference; the caller's depth guard catches a misconfigured cycle.
      {"boot-conf", "${etc}/nsclient/boot.ini"},
#endif
  };

  const auto it = defaults.find(key);
  return it == defaults.end() ? std::string() : it->second;
}

// Substitute every ${token} in `file` using `resolve`, repeatedly, since a
// default may itself be written in terms of another token
// (${certificate-path} -> ${shared-path}/security -> ...).
//
// `depth_limit` caps the cycle defence: "${a}" -> "${b}" -> "${a}" would
// otherwise loop forever. Callers with a logger should prefer their own
// expander so a cycle can be reported; this one just stops.
//
// The core has its own parser-based implementation; this exists for the
// standalone clients, which cannot link it. It replaces a hand-rolled version
// that extracted the key with `substr(pstart + 1, pend - 2)` - correct only
// when the token starts the string, and silently wrong (yielding a key like
// "etc}/x.pem", which resolved to the executable's directory) for anything
// else.
template <typename Resolver>
std::string expand_tokens(std::string file, const Resolver &resolve, const int depth_limit = 32) {
  for (int depth = 0; depth < depth_limit; ++depth) {
    const std::string::size_type start = file.find("${");
    if (start == std::string::npos) return file;
    const std::string::size_type end = file.find('}', start + 2);
    if (end == std::string::npos) return file;  // unterminated - leave it alone

    const std::string key = file.substr(start + 2, end - start - 2);
    const std::string value = resolve(key);
    // A token that resolves to itself would spin here; treating it as opaque
    // and moving on is better than looping until the depth limit.
    if (value == file.substr(start, end - start + 1)) return file;
    file = file.substr(0, start) + value + file.substr(end + 1);
  }
  return file;
}

}  // namespace paths
}  // namespace nscp
