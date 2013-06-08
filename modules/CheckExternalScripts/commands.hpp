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
			, user(other.user)
			, domain(other.domain)
			, password(other.password)
			, encoding(other.encoding)
		{}
		const command_object& operator =(const command_object &other) {
			path = other.path;
			alias = other.alias;
			value = other.value;
			parent = other.parent;
			is_template = other.is_template;
			command = other.command;
			user = other.user;
			domain = other.domain;
			password = other.password;
			encoding = other.encoding;
			return *this;
		}
	
		// Object keys (managed by object handler)
		std::string path;
		std::string alias;
		std::string value;
		std::string parent;
		bool is_template;

		// Command keys
		std::string encoding;
		std::string command;
		std::string user, domain, password;

		std::string to_string() const {
			std::stringstream ss;
			ss << alias << "[" << alias << "] = "
				<< "{command: " << command;
			if (!user.empty()) {
				ss << ", user: " << user 
				<< ", domain: " << domain 
				<< ", password: " << password;
			}
			ss << "}";
			return ss.str();
		}

		void set_command(std::string str) {
			command = str;
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
			// Pupulate default template!

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
				("COMMAND DEFENITION", "Command definition for: " + object.alias)
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


				("user", nscapi::settings_helper::string_key(&object.user),
				"USER", "The user to run the command as", true)

				("domain", nscapi::settings_helper::string_key(&object.domain),
				"DOMAIN", "The user to run the command as", true)

				("password", nscapi::settings_helper::string_key(&object.password),
				"PASSWORD", "The user to run the command as", true)

				("encoding", nscapi::settings_helper::string_key(&object.encoding),
				"ENCODING", "The encoding to parse the command as", true)

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
			import_string(object.encoding, parent.encoding);
		}

	};
	typedef nscapi::settings_objects::object_handler<command_object, command_reader> command_handler;
}

