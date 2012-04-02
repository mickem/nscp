#pragma once

#include <map>
#include <string>

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
		std::wstring command;
		std::list<std::wstring> arguments;
		std::wstring user, domain, password;


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
				<< _T(", arguments: ") << get_argument();
				if (!user.empty()) {
					ss << _T(", user: ") << user 
					<< _T(", domain: ") << domain 
					<< _T(", password: ") << password;
				}
				ss << _T("}");
			return ss.str();
		}

		void set_command(std::wstring str) {
			try {
				strEx::parse_command(str, command, arguments);
			} catch (const std::exception &e) {
				NSC_LOG_ERROR(_T("Failed to parse arguments for command '") + alias + _T("', using old split string method: ") + utf8::to_unicode(e.what()) + _T(": ") + str);
				strEx::splitList list = strEx::splitEx(str, _T(" "));
				if (list.size() > 0) {
					command = list.front();
					list.pop_front();
				}
				arguments.clear();
				BOOST_FOREACH(std::wstring s, list) {
					arguments.push_back(s);
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

		static void post_process_object(object_type &object) {}


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
			object.set_command(object.value);
			//if (object.alias == _T("default"))
			// Pupulate default template!

			nscapi::settings_helper::settings_registry settings(proxy);

			/*
			object_type::options_type options;
			*/
			/*
			settings.path(object.path).add_path()
				(object.alias, nscapi::settings_helper::wstring_map_path(&options), 
				_T("TARGET DEFENITION"), _T("Target definition for: ") + object.alias)

				;
				*/

			settings.path(object.path).add_path()
				(_T("COMMAND DEFENITION"), _T("Command definition for: ") + object.alias)
				;

			settings.path(object.path).add_key()
				(_T("command"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_command, &object, _1)),
				_T("COMMAND"), _T("Command to execute"))

				(_T("alias"), sh::wstring_key(&object.alias),
				_T("ALIAS"), _T("The alias (service name) to report to server"))

				(_T("parent"), nscapi::settings_helper::wstring_key(&object.parent, _T("default")),
				_T("PARENT"), _T("The parent the target inherits from"), true)

				(_T("is template"), nscapi::settings_helper::bool_key(&object.is_template, false),
				_T("IS TEMPLATE"), _T("Declare this object as a template (this means it will not be available as a separate object)"), true)

				(_T("user"), nscapi::settings_helper::wstring_key(&object.user),
				_T("USER"), _T("The user to run the command as"), true)

				(_T("domain"), nscapi::settings_helper::wstring_key(&object.domain),
				_T("DOMAIN"), _T("The user to run the command as"), true)

				(_T("password"), nscapi::settings_helper::wstring_key(&object.password),
				_T("PASSWORD"), _T("The user to run the command as"), true)

				;

			settings.register_all();
			settings.notify();

			/*
			BOOST_FOREACH(const object_type::options_type::value_type &kvp, options) {
				if (!object.has_option(kvp.first))
					object.options[kvp.first] = kvp.second;
			}
			*/

		}

		static void apply_parent(object_type &object, object_type &parent) {
			import_string(object.user, parent.user);
			import_string(object.domain, parent.domain);
			import_string(object.password, parent.password);
			import_string(object.command, parent.command);
			//import_string(object.arguments, parent.arguments);
			if (object.arguments.empty() && !parent.arguments.empty())
				object.arguments = parent.arguments;
			/*
			object.address.import(parent.address);
			BOOST_FOREACH(object_type::options_type::value_type i, parent.options) {
				if (object.options.find(i.first) == object.options.end())
					object.options[i.first] = i.second;
			}
			*/
		}

	};
	typedef nscapi::settings_objects::object_handler<command_object, command_reader> command_handler;
}

