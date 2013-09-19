#pragma once

#include <map>
#include <string>
#include <algorithm>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

namespace sh = nscapi::settings_helper;

namespace alias {
	struct command_object {

		command_object() : is_template(false) {}
		command_object(const command_object &other) 
			: path(other.path)
			, alias(other.alias)
			, value(other.value)
			, parent(other.parent)
			, is_template(other.is_template)
			, command(other.command)
			, arguments(other.arguments)
		{}
		const command_object& operator =(const command_object &other) {
			path = other.path;
			alias = other.alias;
			value = other.value;
			parent = other.parent;
			is_template = other.is_template;
			command = other.command;
			arguments = other.arguments;
			return *this;
		}
	
		// Object keys (managed by object handler)
		std::string path;
		std::string alias;
		std::string value;
		std::string parent;
		bool is_template;

		// Command keys
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
				<< "{command: " << command
				<< ", arguments: ";
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

	template<class T>
	inline void import_string(T &object, T &parent) {
		if (object.empty() && !parent.empty())
			object = parent;
	}


	struct command_reader {
		typedef command_object object_type;

		static void post_process_object(object_type &object) {
			std::transform(object.alias.begin(), object.alias.end(), object.alias.begin(), ::tolower);
		}


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample) {
			object.set_command(object.value);
			std::string alias;
			//if (object.alias == _T("default"))
			// Populate default template!

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(object.path);
			if (is_sample)
				root_path.set_sample();

			if (oneliner) {
				std::string::size_type pos = object.path.find_last_of("/");
				if (pos != std::string::npos) {
					std::string path = object.path.substr(0, pos);
					std::string key = object.path.substr(pos+1);
					proxy->register_key(path, key, NSCAPI::key_string, object.alias, "Alias for " + object.alias + ". To configure this item add a section called: " + object.path, "", false, false);
					proxy->set_string(path, key, object.value);
					return;
				}
			}


			root_path.add_path()
				("ALIAS DEFENITION", "Alias definition for: " + object.alias)
				;

			root_path.add_key()
				("command", sh::string_fun_key<std::string>(boost::bind(&object_type::set_command, &object, _1)),
				"COMMAND", "Command to execute")

				("alias", sh::string_key(&alias),
				"ALIAS", "The alias (service name) to report to server", true)

				("parent", nscapi::settings_helper::string_key(&object.parent, "default"),
				"PARENT", "The parent the target inherits from", true)

				("is template", nscapi::settings_helper::bool_key(&object.is_template, false),
				"IS TEMPLATE", "Declare this object as a template (this means it will not be available as a separate object)", true)

				;

			settings.register_all();
			settings.notify();
			if (!alias.empty())
				object.alias = alias;
		}

		static void apply_parent(object_type &object, object_type &parent) {
			import_string(object.command, parent.command);
			if (object.arguments.empty() && !parent.arguments.empty())
				object.arguments = parent.arguments;
		}

	};
	typedef nscapi::settings_objects::object_handler<command_object, command_reader> command_handler;
}

