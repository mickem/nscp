#pragma once

#include <map>
#include <string>
#include <algorithm>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

namespace sh = nscapi::settings_helper;

namespace commands {
	struct command_object {

		command_object() {}

		nscapi::settings_objects::template_object tpl;
		std::string encoding;
		std::string command;
		std::string user;
		std::string domain;
		std::string password;

		std::string to_string() const {
			std::stringstream ss;
			ss << tpl.alias << "[" << tpl.alias << "] = "
				<< "{tpl: " << tpl.to_string();
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

	struct command_reader {
		typedef command_object object_type;

		static void post_process_object(object_type &object) {
			std::transform(object.tpl.alias.begin(), object.tpl.alias.end(), object.tpl.alias.begin(), ::tolower);
		}
		static void init_default(object_type&) {}

		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample) {
			object.set_command(object.tpl.value);
			std::string alias;
			//if (object.alias == _T("default"))
			// Populate default template!

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
				("COMMAND DEFENITION", "Command definition for: " + object.tpl.alias)
				;

			root_path.add_key()
				("command", sh::string_fun_key<std::string>(boost::bind(&object_type::set_command, &object, _1)),
				"COMMAND", "Command to execute")

				("user", nscapi::settings_helper::string_key(&object.user),
				"USER", "The user to run the command as", true)

				("domain", nscapi::settings_helper::string_key(&object.domain),
				"DOMAIN", "The user to run the command as", true)

				("password", nscapi::settings_helper::string_key(&object.password),
				"PASSWORD", "The user to run the command as", true)

				("encoding", nscapi::settings_helper::string_key(&object.encoding),
				"ENCODING", "The encoding to parse the command as", true)
				;

			object.tpl.read_object(root_path);

			settings.register_all();
			settings.notify();
			if (!alias.empty())
				object.tpl.alias = alias;
		}

		static void apply_parent(object_type &object, object_type &parent) {
			using namespace nscapi::settings_objects;
			import_string(object.user, parent.user);
			import_string(object.domain, parent.domain);
			import_string(object.password, parent.password);
			import_string(object.command, parent.command);
			import_string(object.encoding, parent.encoding);
		}

	};
	typedef nscapi::settings_objects::object_handler<command_object, command_reader> command_handler;
}

