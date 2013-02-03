#pragma once

#include <map>
#include <string>
#include <algorithm>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

namespace sh = nscapi::settings_helper;

namespace commands {
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
			, user(other.user)
			, domain(other.domain)
			, password(other.password)
		{}
		const command_object& operator =(const command_object &other) {
			path = other.path;
			alias = other.alias;
			value = other.value;
			parent = other.parent;
			is_template = other.is_template;
			command = other.command;
			arguments = other.arguments;
			user = other.user;
			domain = other.domain;
			password = other.password;
			return *this;
		}
	
		// Object keys (managed by object handler)
		std::wstring path;
		std::wstring alias;
		std::wstring value;
		std::wstring parent;
		bool is_template;

		// Command keys
		std::string command;
		std::list<std::string> arguments;
		std::string user, domain, password;

		/*
		std::wstring get_argument() const {
			std::wstring args;
			BOOST_FOREACH(std::wstring s, arguments) {
				if (!args.empty())
					args += _T(" ");
				args += s;
			}
			return args;
		}

		std::wstring to_wstring() const {
			std::wstringstream ss;
			ss << alias << _T("[") << alias << _T("] = ") 
				<< _T("{command: ") << command
				<< _T(", arguments: ");
			bool first = true;
			BOOST_FOREACH(const std::wstring &s, arguments) {
				if (first)
					first = false;
				else 
					ss << L',';
				ss << s;
			}
			if (!user.empty()) {
				ss << _T(", user: ") << user 
				<< _T(", domain: ") << domain 
				<< _T(", password: ") << password;
			}
			ss << _T("}");
			return ss.str();
		}
		*/

		void set_command(std::string str) {
			if (str.empty())
				return;
			try {
				strEx::s::parse_command(str, arguments);
				if (arguments.size() > 0) {
					command = arguments.front(); arguments.pop_front();
				}
			} catch (const std::exception &e) {
				//NSC_LOG_MESSAGE(_T("Failed to parse arguments for command '") + alias + _T("', using old split string method: ") + utf8::to_unicode(e.what()) + _T(": ") + str);
				std::list<std::string> list = strEx::s::splitEx(str, std::string(" "));
				if (list.size() > 0) {
					command = list.front();
					list.pop_front();
				}
				arguments.clear();
				std::list<std::string> buffer;
				BOOST_FOREACH(std::string s, list) {
					std::size_t len = s.length();
					if (buffer.empty()) {
						if (len > 2 && s[0] == L'\"' && s[len-1]  == L'\"') {
							buffer.push_back(s.substr(1, len-2));
						} else if (len > 1 && s[0] == L'\"') {
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


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner) {
			object.set_command(utf8::cvt<std::string>(object.value));
			std::wstring alias;
			//if (object.alias == _T("default"))
			// Pupulate default template!

			nscapi::settings_helper::settings_registry settings(proxy);

			if (oneliner) {
				std::wstring::size_type pos = object.path.find_last_of(_T("/"));
				if (pos != std::wstring::npos) {
					std::wstring path = object.path.substr(0, pos);
					std::wstring key = object.path.substr(pos+1);
					proxy->register_key(path, key, NSCAPI::key_string, object.alias, _T("Alias for ") + object.alias + _T(". To configure this item add a section called: ") + object.path, _T(""), false);
					proxy->set_string(path, key, object.value);
					return;
				}
			}
			settings.path(object.path).add_path()
				(_T("COMMAND DEFENITION"), _T("Command definition for: ") + object.alias)
				;

			settings.path(object.path).add_key()
				(_T("command"), sh::string_fun_key<std::string>(boost::bind(&object_type::set_command, &object, _1)),
				_T("COMMAND"), _T("Command to execute"))

				(_T("alias"), sh::wstring_key(&alias),
				_T("ALIAS"), _T("The alias (service name) to report to server"), true)

				(_T("parent"), nscapi::settings_helper::wstring_key(&object.parent, _T("default")),
				_T("PARENT"), _T("The parent the target inherits from"), true)

				(_T("is template"), nscapi::settings_helper::bool_key(&object.is_template, false),
				_T("IS TEMPLATE"), _T("Declare this object as a template (this means it will not be available as a separate object)"), true)

				(_T("user"), nscapi::settings_helper::string_key(&object.user),
				_T("USER"), _T("The user to run the command as"), true)

				(_T("domain"), nscapi::settings_helper::string_key(&object.domain),
				_T("DOMAIN"), _T("The user to run the command as"), true)

				(_T("password"), nscapi::settings_helper::string_key(&object.password),
				_T("PASSWORD"), _T("The user to run the command as"), true)

				;

			settings.register_all();
			settings.notify();
			if (!alias.empty())
				object.alias = alias;
		}

		static void apply_parent(object_type &object, object_type &parent) {
			import_string(object.user, parent.user);
			import_string(object.domain, parent.domain);
			import_string(object.password, parent.password);
			import_string(object.command, parent.command);
			if (object.arguments.empty() && !parent.arguments.empty())
				object.arguments = parent.arguments;
		}

	};
	typedef nscapi::settings_objects::object_handler<command_object, command_reader> command_handler;
}

