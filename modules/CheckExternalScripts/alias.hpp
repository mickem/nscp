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
		std::wstring path;
		std::wstring alias;
		std::wstring value;
		std::wstring parent;
		bool is_template;

		// Command keys
		std::wstring command;
		std::list<std::wstring> arguments;

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
			ss << _T("}");
			return ss.str();
		}

		void set_command(std::wstring str) {
			if (str.empty())
				return;
			try {
				strEx::parse_command(str, command, arguments);
			} catch (const std::exception &e) {
				NSC_LOG_MESSAGE(_T("Failed to parse arguments for command '") + alias + _T("', using old split string method: ") + utf8::to_unicode(e.what()) + _T(": ") + str);
				strEx::splitList list = strEx::splitEx(str, _T(" "));
				if (list.size() > 0) {
					command = list.front();
					list.pop_front();
				}
				arguments.clear();
				std::list<std::wstring> buffer;
				BOOST_FOREACH(std::wstring s, list) {
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
						if (len > 1 && s[len-1] == L'\"') {
							std::wstring tmp;
							BOOST_FOREACH(const std::wstring &s2, buffer) {
								if (tmp.empty()) {
									tmp = s2.substr(1);
								} else {
									tmp += _T(" ") + s2;
								}
							}
							arguments.push_back(tmp + _T(" ") + s.substr(0, len-1));
							buffer.clear();
						} else {
							buffer.push_back(s);
						}
					}
				}
				if (!buffer.empty()) {
					BOOST_FOREACH(const std::wstring &s, buffer) {
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
			object.set_command(object.value);
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
				(_T("ALIAS DEFENITION"), _T("Alias definition for: ") + object.alias)
				;

			settings.path(object.path).add_key()
				(_T("command"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_command, &object, _1)),
				_T("COMMAND"), _T("Command to execute"))

				(_T("alias"), sh::wstring_key(&alias),
				_T("ALIAS"), _T("The alias (service name) to report to server"), true)

				(_T("parent"), nscapi::settings_helper::wstring_key(&object.parent, _T("default")),
				_T("PARENT"), _T("The parent the target inherits from"), true)

				(_T("is template"), nscapi::settings_helper::bool_key(&object.is_template, false),
				_T("IS TEMPLATE"), _T("Declare this object as a template (this means it will not be available as a separate object)"), true)

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

