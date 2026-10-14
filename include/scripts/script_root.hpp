// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The `script root` option shared by every module that carries scripts
// (CheckExternalScripts, PythonScript, LUAScript, CheckMKClient and
// CheckMKServer): one value that says where a module's scripts tree hangs
// from, resolved here so the modules cannot disagree about what it means.
//
// A root is the *parent* of the `scripts` folder, not the folder itself, and
// that is what makes a tree relocatable: an external script is started with
// the root as its working directory and a relative command (`scripts\a.bat`)
// is rooted there, so a script that calls a sibling as `scripts\b.bat` keeps
// working wherever the tree lives. Python and Lua look in `<root>/scripts/
// python` and `<root>/scripts/lua`, so a tree laid out the way the install
// directory is can be dropped anywhere - which is exactly what a fleet
// bundle's `scripts/` entries are: the sync stages them under
// `${fleet-folder}/scripts`, and `script root = fleet` points the modules at
// them.
//
// The value is one of:
//
//   (empty) or `default`   the built-in layout: scripts in ${scripts}, external
//                          scripts rooted at ${base-path} as they always were
//   `fleet`                ${fleet-folder}, the tree the fleet sync owns
//   `shared`               ${shared-path}, the writable state directory on the
//                          modern Windows layout
//   a path                 any folder, ${...} tokens expanded, holding a
//                          `scripts` sub-folder
//
// The built-in layout is deliberately not expressed as a path: ${scripts} is
// its own [paths] token and need not be called `scripts` (an override can
// point it anywhere), so deriving it from a parent would move it. Only a
// configured root imposes the `<root>/scripts` shape.

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/path.hpp>
#include <stdexcept>
#include <string>

namespace scripts {
namespace root {

struct layout {
  // The folder the scripts tree hangs from: the working directory an external
  // script runs in and the base a relative command is rooted at. Empty for
  // the built-in layout, where the launcher keeps its historical root.
  boost::filesystem::path root;
  // The scripts folder: ${scripts} for the built-in layout, `<root>/scripts`
  // for a configured one. Python and Lua append their own sub-folder to it.
  boost::filesystem::path scripts;

  bool custom() const { return !root.empty(); }
};

inline const char *fleet_alias() { return "fleet"; }
inline const char *shared_alias() { return "shared"; }
inline const char *default_alias() { return "default"; }

// The token an alias stands for, or the value itself when it is not one.
inline std::string alias_target(const std::string &value) {
  const std::string lower = boost::algorithm::to_lower_copy(value);
  if (lower == fleet_alias()) return "${fleet-folder}";
  if (lower == shared_alias()) return "${shared-path}";
  return value;
}

// Whether `value` selects the built-in layout.
inline bool is_default(const std::string &value) {
  const std::string lower = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(value));
  return lower.empty() || lower == default_alias();
}

// A path with any trailing separator dropped, so `<root>/scripts` is spelled
// the same whichever way the operator wrote the root.
inline boost::filesystem::path without_trailing_separator(boost::filesystem::path p) {
  std::string s = p.string();
  while (s.size() > 1 && (s.back() == '/' || s.back() == '\\')) s.pop_back();
  return boost::filesystem::path(s);
}

// Resolve the configured `script root`. `expand` turns a string with ${...}
// tokens into a path (the core's expand_path); it is handed the aliases'
// targets and the value itself, never a bare alias, so an unknown token is
// reported the way it is for every other path option.
template <class Expand>
layout resolve(const std::string &configured, Expand expand) {
  layout result;
  const std::string value = boost::algorithm::trim_copy(configured);
  if (is_default(value)) {
    result.scripts = boost::filesystem::path(expand("${scripts}"));
    return result;
  }
  result.root = without_trailing_separator(boost::filesystem::path(expand(alias_target(value))));
  result.scripts = result.root / "scripts";
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
  return "Where this module's scripts live and, for external scripts, the directory they run in. The default (empty, or `default`) is the "
         "built-in layout: ${scripts} for the scripts and the installation directory as the working directory of an external script. `fleet` "
         "is ${fleet-folder}, the tree the fleet sync keeps - a bundle's `scripts/` entries land there, so a bundle that ships scripts sets "
         "this to `fleet` and refers to them as `scripts\\<name>`. `shared` is ${shared-path}; any other value is a folder (${...} tokens "
         "expanded). A configured root always has the install layout below it: `<root>/scripts` holds the scripts, `<root>/scripts/python` "
         "and `<root>/scripts/lua` the Python and Lua ones, and an external script is started with `<root>` as its working directory with a "
         "relative command such as `scripts\\check.bat` resolved against it - so scripts that call each other by relative path keep working "
         "wherever the tree is placed. Set it under [/settings/default] to apply to every script module at once. SECURITY: whoever can write "
         "to the root can change what the service runs.";
}

}  // namespace root
}  // namespace scripts
