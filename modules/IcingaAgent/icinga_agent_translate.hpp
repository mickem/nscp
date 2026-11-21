// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Icinga check commands the agent answers under their Icinga names.
//
// A host that ran the Icinga 2 agent has services on the commands of the
// Icinga Template Library: `disk-windows`, `load-windows`, `nscp-local-cpu`.
// The master sends such a check as the command's name and the macros its
// arguments use (`disk_win_warn`, `disk_win_path`, ...) - what the Icinga
// agent needed to build the plugin's command line. This builds the NSClient++
// query from the same two things, so the services keep working unchanged when
// NSClient++ takes the agent's place.
//
// Two sources, looked up in this order:
//   - aliases from the configuration: `name = query argument...`, where an
//     argument may use the macros the master sends (`drive=$disk_win_path$`);
//   - the built-in translations of the ITL's Windows plugins and `nscp-local`
//     commands, which also convert the plugins' thresholds (see
//     icinga_agent_thresholds.hpp) into the -w / -c of the query.

#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace icinga_agent {

// The command is known but this use of it cannot be translated: a threshold
// the plugin would have refused, an option without a counterpart. Reported as
// UNKNOWN on the check rather than run as something else.
struct translation_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// `name = query argument...` from the configuration, already split.
struct command_alias {
  std::string query;
  std::list<std::string> arguments;
};

struct translation_config {
  bool builtin = true;  // translate the ITL commands listed by builtin_translations()
  std::map<std::string, command_alias> aliases;
};

struct translated_command {
  std::string query;
  std::list<std::string> arguments;
  // The arguments are the alias's own text, with nothing from the master in
  // them: such an alias runs even where the master may not pass arguments.
  bool fixed_arguments = false;
};

// The query for an Icinga command, or none when neither an alias nor a
// built-in translation carries that name. Throws translation_error,
// argument_error or macro_syntax_error.
boost::optional<translated_command> translate_command(const translation_config &config, const std::string &command, const boost::json::object &macros);

// The Icinga commands with a built-in translation, sorted.
std::vector<std::string> builtin_translations();

}  // namespace icinga_agent
