// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "command_hints.hpp"

#include <boost/algorithm/string.hpp>
#include <command_module_map.hpp>
#include <str/format.hpp>

namespace nsclient {
namespace core {
namespace command_hints {

std::vector<std::string> providers_of(const std::string &command) {
  const std::string key = boost::algorithm::to_lower_copy(command);
  std::vector<std::string> ret;
  for (const generated::command_module_entry *e = generated::command_module_map; e->command != nullptr; ++e) {
    if (key == e->command) ret.emplace_back(e->module);
  }
  return ret;
}

std::string describe(const std::vector<std::string> &providers, const state_lookup &state) {
  // A loaded provider that does not have the command has nothing to offer:
  // enabling something else would not help either.
  std::vector<std::pair<std::string, module_state>> candidates;
  for (const std::string &module : providers) {
    const module_state s = state(module);
    if (s == module_state::loaded) return {};
    candidates.emplace_back(module, s);
  }
  if (candidates.empty()) return {};
  if (candidates.size() > 1) {
    std::string names;
    for (const auto &c : candidates) str::format::append_list(names, c.first, " or ");
    return "provided by modules " + names + ", none of which is loaded: enable one of them under [/modules] and restart";
  }
  const std::string &module = candidates.front().first;
  switch (candidates.front().second) {
    case module_state::failed_to_load:
      return "provided by module " + module + ", which is enabled but failed to load: see the log for why";
    case module_state::enabled_not_loaded:
      return "provided by module " + module + ", which is enabled but not loaded";
    case module_state::not_enabled:
    default:
      return "provided by module " + module + ", which is not loaded: add \"" + module + " = enabled\" under [/modules] and restart";
  }
}

std::string annotate(const std::string &command, const std::vector<std::string> &providers, const state_lookup &state) {
  const std::string hint = describe(providers, state);
  if (hint.empty()) return command;
  return command + " (" + hint + ")";
}

std::string unknown_commands_message(const std::string &prefix, const std::list<std::string> &commands, const provider_lookup &providers,
                                     const state_lookup &state) {
  std::string list;
  for (const std::string &command : commands) str::format::append_list(list, annotate(command, providers(command), state));
  return prefix + list;
}

}  // namespace command_hints
}  // namespace core
}  // namespace nsclient
