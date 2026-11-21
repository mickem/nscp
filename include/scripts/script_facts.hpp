// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <ctime>
#include <nscapi/nscapi_facts_helper.hpp>
#include <string>
#include <vector>

// The `scripts` fact set: what each script runtime has in its scripts folder.
//
//   scripts.external   CheckExternalScripts
//   scripts.python     PythonScript
//   scripts.lua        LUAScript
//
// An on-demand set (nscapi::facts::response::on_demand): the listing is read
// off the folder when someone asks for it, and is never stored in the facts
// document or uploaded. The folder changes whenever a script is added, so a
// stored copy would be stale the moment it mattered; and what a host can be
// made to run is not something an inventory should be shipping around.
//
// Each module owns one key of the set, so the three can be loaded in any
// combination and `GET /api/v2/facts?path=scripts` answers with whichever are.
//
// It is the listing only. Names, never contents: the source of a script is
// not a fact, and is still read through the scripts API under its own grant.
// Nor is a script's command line listed - for the external runtime `commands`
// carries the names the scripts are registered under and nothing they run.
namespace scripts {
namespace facts {

constexpr const char *const set_scripts = "scripts";
constexpr const char *const key_external = "external";
constexpr const char *const key_python = "python";
constexpr const char *const key_lua = "lua";
// A list in a fact set holds at most 5000 entries (the document rules), and a
// set that breaks a rule is refused whole.
constexpr std::size_t max_entries = 5000;

// The id a module declares and is asked for: `scripts.python`.
inline std::string id_of(const std::string &key) { return std::string(set_scripts) + "." + key; }

// What one runtime lists. Owns its strings: filled from the folder walk and
// handed over whole, so nothing here outlives a directory iterator.
struct listing {
  // The script files, spelled the way the runtime's `add` and the scripts API
  // take them back, in the order the folder walk found them.
  std::vector<std::string> files;
  // The names the runtime's scripts are registered under as commands. Only
  // the external runtime has such a list of its own.
  std::vector<std::string> commands;
  bool has_commands = false;
};

namespace detail {
inline std::vector<std::string> capped(const std::vector<std::string> &values) {
  if (values.size() <= max_entries) return values;
  return std::vector<std::string>(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(max_entries));
}
}  // namespace detail

// Write one runtime's listing under `scripts.<key>`.
//
// An empty folder is an empty list, not an absent one: the runtime is loaded
// and has nothing, which is a different answer from "not loaded".
inline void publish(const std::string &key, const listing &what, const std::time_t taken_at, nscapi::facts::response &out) {
  nscapi::facts::section runtime = out.set(set_scripts).object(key);
  runtime.strings("files", detail::capped(what.files));
  if (what.has_commands) runtime.strings("commands", detail::capped(what.commands));
  const std::size_t longest = what.has_commands && what.commands.size() > what.files.size() ? what.commands.size() : what.files.size();
  if (longest > max_entries) {
    out.error(id_of(key), "Script listing truncated: found " + std::to_string(longest) + ", reporting " + std::to_string(max_entries));
  }
  out.gathered(set_scripts, taken_at);
}

}  // namespace facts
}  // namespace scripts
