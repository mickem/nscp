/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/algorithm/string/case_conv.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/object.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <string>

namespace sh = nscapi::settings_helper;

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
typedef boost::shared_ptr<command_object> command_object_instance;

typedef nscapi::settings_objects::object_handler<command_object> command_handler;
}  // namespace alias