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
// (build/python/create_command_module_map.py); an unknown command found there
// names its module. Commands registered at runtime (scripts, aliases) are not
// in the map and get no hint.
namespace nsclient {
namespace core {
namespace command_hints {

typedef std::function<std::vector<std::string>(const std::string &command)> provider_lookup;

// The modules the build says provide `command` (matched case-insensitively,
// like the command registry). Empty for a command no built module declares.
std::vector<std::string> providers_of(const std::string &command);

// `command`, followed in parentheses by the module(s) to enable when there
// are any.
std::string annotate(const std::string &command, const std::vector<std::string> &providers);

// `prefix` followed by every command, each annotated, separated by ", ". The
// prefix ("Unknown command(s): ") is kept verbatim so scripts matching on it
// still work.
std::string unknown_commands_message(const std::string &prefix, const std::list<std::string> &commands, const provider_lookup &providers = providers_of);

}  // namespace command_hints
}  // namespace core
}  // namespace nsclient
