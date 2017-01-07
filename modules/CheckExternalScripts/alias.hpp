/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <map>
#include <string>
#include <algorithm>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace sh = nscapi::settings_helper;

namespace alias {
	struct command_object : public nscapi::settings_objects::object_instance_interface {
		typedef nscapi::settings_objects::object_instance_interface parent;

		command_object(std::string alias, std::string path)
			: parent(alias, path) {}

		std::string command;
		std::list<std::string> arguments;

		std::string get_argument() const {
			std::string args;
			BOOST_FOREACH(const std::string &s, arguments) {
				if (!args.empty())
					args += " ";
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
			BOOST_FOREACH(const std::string &s, arguments) {
				if (first)
					first = false;
				else
					ss << ',';
				ss << s;
			}
			ss << "}";
			return ss.str();
		}

		void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);
			set_alias(boost::algorithm::to_lower_copy(get_alias()));

			set_command(get_value());

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			if (oneliner)
				return;

			root_path.add_path()
				("alias: " + get_alias(), "The configuration section for the " + get_alias() + " alias")
				;

			root_path.add_key()
				("command", sh::string_fun_key(boost::bind(&command_object::set_command, this, _1)),
					"COMMAND", "Command to execute")
				;


			settings.register_all();
			settings.notify();
		}

		void set_command(std::string str) {
			if (str.empty())
				return;
			try {
				strEx::parse_command(str, command, arguments);
			} catch (const std::exception &e) {
				NSC_LOG_MESSAGE("Failed to parse arguments for command using old split string method: " + utf8::utf8_from_native(e.what()) + ": " + str);
				std::list<std::string> list = strEx::s::splitEx(str, std::string(" "));
				if (list.size() > 0) {
					command = list.front();
					list.pop_front();
				}
				arguments.clear();
				std::list<std::string> buffer;
				BOOST_FOREACH(const std::string &s, list) {
					std::size_t len = s.length();
					if (buffer.empty()) {
						if (len > 2 && s[0] == '\"' && s[len - 1] == '\"') {
							buffer.push_back(s.substr(1, len - 2));
						} else if (len > 1 && s[0] == '\"') {
							buffer.push_back(s);
						} else {
							arguments.push_back(s);
						}
					} else {
						if (len > 1 && s[len - 1] == '\"') {
							std::string tmp;
							BOOST_FOREACH(const std::string &s2, buffer) {
								if (tmp.empty()) {
									tmp = s2.substr(1);
								} else {
									tmp += " " + s2;
								}
							}
							arguments.push_back(tmp + " " + s.substr(0, len - 1));
							buffer.clear();
						} else {
							buffer.push_back(s);
						}
					}
				}
				if (!buffer.empty()) {
					BOOST_FOREACH(const std::string &s, buffer) {
						arguments.push_back(s);
					}
				}
			}
		}
	};
	typedef boost::shared_ptr<command_object> command_object_instance;

	typedef nscapi::settings_objects::object_handler<command_object> command_handler;
}