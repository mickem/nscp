// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Shared command-alias machinery. Used by CheckExternalScripts and
// CheckHelpers to provide [/settings/.../alias] sections of the form
//     my_alias = check_target arg1 arg2
// Originally lived next to CheckExternalScripts; moved here so admins can
// run aliases without enabling external-script execution.

#pragma once

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/optional.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/kvp_map.hpp>
#include <nscapi/settings/object.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <string>

namespace alias {
struct command_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  command_object(std::string alias, std::string path) : parent(alias, path) {}

  std::string command;
  std::list<std::string> arguments;

  std::string get_argument() const {
    std::string args;
    for (const std::string &s : arguments) {
      if (!args.empty()) args += " ";
      args += s;
    }
    return args;
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << get_alias() << "[" << get_alias() << "] = "
       << "{tpl: " << parent::to_string();
    ss << ", command: " << command << ", arguments: ";
    bool first = true;
    for (const std::string &s : arguments) {
      if (first)
        first = false;
      else
        ss << ',';
      ss << s;
    }
    ss << "}";
    return ss.str();
  }

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);
    set_alias(boost::algorithm::to_lower_copy(get_alias()));

    set_command(get_value());

    nscapi::settings_helper::settings_registry settings(proxy);
    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    if (oneliner) return;

    root_path.add_path()("alias: " + get_alias(), "The configuration section for the " + get_alias() + " alias");

    namespace sh = nscapi::settings_helper;
    root_path.add_key().add_string("command", sh::string_fun_key([this](auto value) { this->set_command(value); }), "COMMAND", "Command to execute");

    settings.register_all();
    settings.notify();
  }

  void set_command(std::string str) {
    if (str.empty()) return;
    try {
      str::utils::parse_command(str, command, arguments);
    } catch (const std::exception &e) {
      // No fallback parser. The previous implementation attempted a naive
      // split-on-space with no quote-escape handling, which silently
      // accepted malformed alias definitions and produced surprising
      // tokenisation - in particular it never honoured `\"` and could
      // splice attacker-influenced bytes across argv elements differently
      // than the principal parser. Refuse to register the alias instead,
      // so an operator sees the failure at startup and fixes their config.
      command.clear();
      arguments.clear();
      NSC_LOG_ERROR_STD("Refusing alias '" + get_alias() + "' - command line did not parse cleanly: " + utf8::utf8_from_native(e.what()) + " (input: " + str +
                        ")");
    }
  }
};
typedef std::shared_ptr<command_object> command_object_instance;

typedef nscapi::settings_objects::object_handler<command_object> command_handler;

// Flat-section alternative to command_handler. Each alias is just a single
// `key = value` line under the parent section - no per-alias subdirectory,
// no parent/is template/alias keys, no object_handler machinery. The trade-off
// is no template inheritance, but aliases are leaf definitions ("name -> command
// line") so the inheritance machinery is dead weight there. CheckHelpers uses
// this; CheckExternalScripts still uses command_handler because its alias
// machinery is shared with the more complex script object section.
struct simple_command {
  std::string name;
  std::string command;
  std::list<std::string> arguments;

  std::string get_alias() const { return name; }
  std::string get_argument() const {
    std::string args;
    for (const std::string &s : arguments) {
      if (!args.empty()) args += " ";
      args += s;
    }
    return args;
  }
};

// Backed by the generic kvp_map; the parser tokenises the value into a
// simple_command and rejects (with a log message) anything that won't parse
// cleanly. Aliases are looked up case-insensitively because incoming command
// names come off the wire where casing isn't guaranteed.
using simple_command_map = nscapi::settings::kvp_map<simple_command>;

inline simple_command_map make_simple_command_map() {
  return simple_command_map(
      [](const std::string &name, const std::string &raw) -> boost::optional<simple_command> {
        simple_command def;
        def.name = name;
        try {
          str::utils::parse_command(raw, def.command, def.arguments);
        } catch (const std::exception &e) {
          NSC_LOG_ERROR_STD("Refusing alias '" + name + "' - command line did not parse cleanly: " + utf8::utf8_from_native(e.what()) + " (input: " + raw +
                            ")");
          return boost::none;
        }
        if (def.command.empty()) return boost::none;
        return def;
      },
      /*case_insensitive=*/true);
}
}  // namespace alias
