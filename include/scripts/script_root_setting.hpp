// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Reading the `script root` option (include/scripts/script_root.hpp) through
// the settings helper: the one registration every script-carrying module
// makes, kept here so the five of them cannot drift apart on the key's name,
// its parent section or its help text.

#include <nscapi/settings/helper.hpp>
#include <scripts/script_root.hpp>
#include <string>

namespace scripts {
namespace root {

// Register the key on `settings`, whose alias is already set: read from the
// module's own section, falling back to [/settings/default] so one line in a
// fleet bundle moves every script module at once. `more` is appended to the
// shared help text for what the key additionally governs in this module.
inline void register_key(nscapi::settings_helper::settings_registry &settings, std::string *value, const std::string &more = "") {
  settings.alias()
      .add_parent("/settings/default")
      .add_key_to_settings()
      .add_string("script root", nscapi::settings_helper::string_key(value, ""), "Script root", description() + more);
}

// Read the key on its own, ahead of everything the module builds around the
// folder (a Lua runtime, a settings walk that loads scripts through it):
// register it, run the walk, clear the registry for the module's own pass,
// and resolve what was read.
template <class Expand, class Report>
layout load(nscapi::settings_helper::settings_registry &settings, Expand expand, Report report) {
  std::string value;
  register_key(settings, &value);
  settings.register_all();
  settings.notify();
  settings.clear();
  return resolve_or_default(value, expand, report);
}

}  // namespace root
}  // namespace scripts
