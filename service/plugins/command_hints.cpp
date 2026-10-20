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

std::string annotate(const std::string &command, const std::vector<std::string> &providers) {
  if (providers.empty()) return command;
  if (providers.size() == 1) {
    const std::string &module = providers.front();
    return command + " (provided by module " + module + ", which is not loaded: add \"" + module + " = enabled\" under [/modules] and restart)";
  }
  std::string names;
  for (const std::string &module : providers) str::format::append_list(names, module, " or ");
  return command + " (provided by modules " + names + ", none of which is loaded: enable one of them under [/modules] and restart)";
}

std::string unknown_commands_message(const std::string &prefix, const std::list<std::string> &commands, const provider_lookup &providers) {
  std::string list;
  for (const std::string &command : commands) str::format::append_list(list, annotate(command, providers(command)));
  return prefix + list;
}

}  // namespace command_hints
}  // namespace core
}  // namespace nsclient
