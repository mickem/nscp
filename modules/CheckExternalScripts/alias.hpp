#pragma once

#include <map>
#include <string>
#include <algorithm>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace sh = nscapi::settings_helper;

namespace alias {
	struct command_object {

		nscapi::settings_objects::template_object tpl;
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
			ss << tpl.to_string() << "{command: " << command << ", arguments: ";
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
	typedef boost::optional<command_object> optional_command_object;

	struct command_reader {
		typedef command_object object_type;

		static void post_process_object(object_type &object) {
			std::transform(object.tpl.alias.begin(), object.tpl.alias.end(), object.tpl.alias.begin(), ::tolower);
		}
		static void init_default(object_type& object) {}


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample) {
			object.set_command(object.tpl.value);
			std::string alias;

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
			if (is_sample)
				root_path.set_sample();

			if (oneliner) {
				std::string::size_type pos = object.tpl.path.find_last_of("/");
				if (pos != std::string::npos) {
					std::string path = object.tpl.path.substr(0, pos);
					std::string key = object.tpl.path.substr(pos+1);
					proxy->register_key(path, key, NSCAPI::key_string, object.tpl.alias, "Alias for " + object.tpl.alias + ". To configure this item add a section called: " + object.tpl.path, "", false, false);
					proxy->set_string(path, key, object.tpl.value);
					return;
				}
			}


			root_path.add_path()
				("ALIAS DEFENITION", "Alias definition for: " + object.tpl.alias)
				;

			root_path.add_key()
				("command", sh::string_fun_key<std::string>(boost::bind(&object_type::set_command, &object, _1)),
				"COMMAND", "Command to execute")
				;
			object.tpl.read_object(root_path);

			settings.register_all();
			settings.notify();
			if (!alias.empty())
				object.tpl.alias = alias;
		}

		static void apply_parent(object_type &object, object_type &parent) {
			using namespace nscapi::settings_objects;
			import_string(object.command, parent.command);
			if (object.arguments.empty() && !parent.arguments.empty())
				object.arguments = parent.arguments;
		}

	};
	typedef nscapi::settings_objects::object_handler<command_object, command_reader> command_handler;
}

