// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The `script root` option shared by every module that carries scripts
// (CheckExternalScripts, PythonScript, LUAScript, CheckMKClient and
// CheckMKServer): the scripts folder, resolved here so the modules cannot
// disagree about what it means.
//
// The value names the scripts folder - what ${scripts} is by default. Python
// and Lua read their `python` and `lua` sub-folders of it, and `ext-scr`,
// `py` and `lua` confine show, delete and import to it. The folder *above* it
// is where an external script is started and the base a relative command
// (`scripts\a.bat`) is rooted at: that is the rule the default layout has
// always followed - ${scripts} is the installation directory's `scripts`
// folder and scripts start in the installation directory - and making it the
// rule for any root is what lets a tree move. Drop the install's scripts
// folder anywhere, point `script root` at it, and `scripts\a.bat` calling
// `scripts\b.bat` keeps working. A fleet bundle's `scripts/` entries are
// staged as `${fleet-folder}/scripts`, which is exactly such a tree.
//
// The value is one of:
//
//   (empty) or `default`   ${scripts}
//   `fleet`                ${fleet-folder}/scripts, the tree the fleet sync owns
//   `shared`               ${shared-path}/scripts
//   a path                 any folder, ${...} tokens expanded

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/path.hpp>
#include <stdexcept>
#include <string>

namespace scripts {
namespace root {

struct layout {
  // The scripts folder: what `script root` names, ${scripts} by default.
  // Python and Lua append their own sub-folder to it.
  boost::filesystem::path scripts;
  // The folder above it: the working directory an external script runs in
  // and the base a relative command is rooted at. Empty when the scripts
  // folder has no parent (a bare relative name).
  boost::filesystem::path base;
  // Whether `script root` names a folder other than ${scripts}. The old
  // default written out (`script root = ${scripts}`, as the documentation
  // once suggested) is not configured: on Windows the built-in layout keeps
  // the installation directory as the start directory even under a
  // `[paths] scripts` override, and that promise has to hold for the value
  // that every pre-existing nsclient.ini was told to carry.
  bool configured = false;
};

inline const char *fleet_alias() { return "fleet"; }
inline const char *shared_alias() { return "shared"; }
inline const char *default_alias() { return "default"; }

// The folder an alias stands for, or the value itself when it is not one.
inline std::string alias_target(const std::string &value) {
  const std::string lower = boost::algorithm::to_lower_copy(value);
  if (lower == fleet_alias()) return "${fleet-folder}/scripts";
  if (lower == shared_alias()) return "${shared-path}/scripts";
  return value;
}

// Whether `value` selects the built-in layout.
inline bool is_default(const std::string &value) {
  const std::string lower = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(value));
  return lower.empty() || lower == default_alias();
}

// A path with any trailing separator dropped, so the parent of `/x/scripts/`
// is `/x` and not `/x/scripts`. A root directory keeps its separator: `C:\`
// stripped to `C:` is the drive-relative "current directory on C", and
// `//server/` to `//server` no directory at all.
inline boost::filesystem::path without_trailing_separator(boost::filesystem::path p) {
  std::string s = p.string();
  while (s.size() > 1 && (s.back() == '/' || s.back() == '\\')) {
    const boost::filesystem::path shorter(s.substr(0, s.size() - 1));
    if (shorter.has_root_name() && !shorter.has_root_directory()) break;
    s.pop_back();
  }
  return boost::filesystem::path(s);
}

// Whether two spellings name the same folder, as far as can be told without
// touching the disk: normalised, trailing separator dropped, and compared the
// way the platform's filesystem compares names.
inline bool same_folder(const boost::filesystem::path &a, const boost::filesystem::path &b) {
  const std::string sa = without_trailing_separator(a.lexically_normal()).generic_string();
  const std::string sb = without_trailing_separator(b.lexically_normal()).generic_string();
#ifdef WIN32
  return boost::algorithm::iequals(sa, sb);
#else
  return sa == sb;
#endif
}

// Resolve the configured `script root`. `expand` turns a string with ${...}
// tokens into a path (the core's expand_path); it is handed the aliases'
// targets and the value itself, never a bare alias, so an unknown token is
// reported the way it is for every other path option.
template <class Expand>
layout resolve(const std::string &configured, Expand expand) {
  layout result;
  const std::string value = boost::algorithm::trim_copy(configured);
  const boost::filesystem::path scripts = without_trailing_separator(boost::filesystem::path(expand("${scripts}")));
  result.scripts = is_default(value) ? scripts : without_trailing_separator(boost::filesystem::path(expand(alias_target(value))));
  result.configured = !same_folder(result.scripts, scripts);
  // The default folder however it was spelled: a `./` or a doubled separator
  // in the written-out value must not leak into the paths the modules use.
  if (!result.configured) result.scripts = scripts;
  result.base = result.scripts.parent_path();
  return result;
}

// resolve(), falling back to the built-in layout when the value cannot be
// expanded (an unknown token, most likely): the module keeps loading with the
// scripts it always had, and `report` is handed the reason for the log.
template <class Expand, class Report>
layout resolve_or_default(const std::string &configured, Expand expand, Report report) {
  try {
    return resolve(configured, expand);
  } catch (const std::exception &e) {
    report("Invalid script root '" + configured + "', using the built-in layout: " + std::string(e.what()));
    return resolve("", expand);
  }
}

// The option's help text, shared so every module documents it identically.
inline std::string description() {
  return "The folder this module's scripts live in: ${scripts} by default (empty, or `default`). `fleet` is ${fleet-folder}/scripts, where the fleet "
         "sync stages a bundle's `scripts/` entries - a bundle that ships scripts sets this to `fleet` and refers to them as `scripts\\<name>`. "
         "`shared` is ${shared-path}/scripts; any other value is a folder (${...} tokens expanded). Python and Lua scripts live in its `python` "
         "and `lua` sub-folders. An external script is started in the folder *above* it, with a relative command such as `scripts\\check.bat` "
         "resolved against that folder - the rule the default layout has always followed, so a scripts folder moved elsewhere runs unchanged, "
         "scripts that call each other by relative path included. Set it under [/settings/default] to apply to every script module at once. "
         "SECURITY: whoever can write to this folder can change what the service runs.";
}

}  // namespace root
}  // namespace scripts
