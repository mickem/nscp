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
	struct command_object : public nscapi::settings_objects::object_instance_interface{

		typedef nscapi::settings_objects::object_instance_interface parent;

		command_object(std::string alias, std::string path) 
			: parent(alias, path)
		{}

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
			ss << alias << "[" << alias << "] = "
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
			set_command(value);

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(path);
			if (is_sample)
				root_path.set_sample();

			if (oneliner) {
				std::string::size_type pos = path.find_last_of("/");
				if (pos != std::string::npos) {
					proxy->register_key(path, alias, NSCAPI::key_string, alias, "Alias for " + alias + ". To configure this item add a section called: " + path + "/" + alias, "", false, false);
					proxy->set_string(path, alias, value);
					return;
				}
			}


			root_path.add_path()
				("ALIAS DEFENITION", "Alias definition for: " + alias)
				;

			root_path.add_key()
				("command", sh::string_fun_key<std::string>(boost::bind(&command_object::set_command, this, _1)),
				"COMMAND", "Command to execute")
				;

			parent::read(proxy, oneliner, is_sample);

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
						if (len > 2 && s[0] == '\"' && s[len-1]  == '\"') {
							buffer.push_back(s.substr(1, len-2));
						} else if (len > 1 && s[0] == '\"') {
							buffer.push_back(s);
						} else {
							arguments.push_back(s);
						}
					} else {
						if (len > 1 && s[len-1] == '\"') {
							std::string tmp;
							BOOST_FOREACH(const std::string &s2, buffer) {
								if (tmp.empty()) {
									tmp = s2.substr(1);
								} else {
									tmp += " " + s2;
								}
							}
							arguments.push_back(tmp + " " + s.substr(0, len-1));
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

