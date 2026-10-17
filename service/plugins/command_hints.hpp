// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <functional>
#include <list>
#include <string>
#include <vector>

// What the core says about a command no loaded module registers.
//
// Most of the time the command exists and its module is simply not enabled
// (check_ping without `CheckNet = enabled`, #698). The build writes a map of
// which module provides which command from every built module's module.json
// (build/python/create_command_module_map.py); this looks the command up there
// and, given what the plugin manager knows about that module, says what to do.
// Commands registered at runtime (scripts, aliases) are not in the map and get
// no hint.
namespace nsclient {
namespace core {
namespace command_hints {

enum class module_state {
  // No [/modules] entry enables it.
  not_enabled,
  // Enabled, but loading or starting it failed.
  failed_to_load,
  // Enabled and not loaded, without a recorded failure: a one-shot
  // `nscp client --module X` loads only X, for instance.
  enabled_not_loaded,
  // Loaded - it just does not register this command - so there is no hint.
  loaded,
};

typedef std::function<std::vector<std::string>(const std::string &command)> provider_lookup;
typedef std::function<module_state(const std::string &module)> state_lookup;

// The modules the build says provide `command` (matched case-insensitively,
// like the command registry). Empty for a command no built module declares.
std::vector<std::string> providers_of(const std::string &command);

// The hint for one command, without the parentheses: empty when no provider is
// known or every provider is loaded.
std::string describe(const std::vector<std::string> &providers, const state_lookup &state);

// `command`, followed by its hint in parentheses when there is one.
std::string annotate(const std::string &command, const std::vector<std::string> &providers, const state_lookup &state);

// `prefix` followed by every command, each annotated, separated by ", ". The
// prefix ("Unknown command(s): ") is kept verbatim so scripts matching on it
// still work.
std::string unknown_commands_message(const std::string &prefix, const std::list<std::string> &commands, const provider_lookup &providers,
                                     const state_lookup &state);

}  // namespace command_hints
}  // namespace core
}  // namespace nsclient
