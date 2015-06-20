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

namespace sh = nscapi::settings_helper;

namespace commands {
	struct command_object : public nscapi::settings_objects::object_instance_interface{

		typedef nscapi::settings_objects::object_instance_interface parent;

		command_object(std::string alias, std::string path) 
			: parent(alias, path)
			, ignore_perf(false) 
		{}

		std::string encoding;
		std::string command;
		std::string user;
		std::string domain;
		std::string password;
		bool ignore_perf;

		std::string to_string() const {
			std::stringstream ss;
			ss << alias << "[" << alias << "] = "
				<< "{tpl: " << parent::to_string();
			if (!user.empty()) {
				ss << ", user: " << user 
				<< ", domain: " << domain 
				<< ", password: " << password;
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
					std::string kpath = path.substr(0, pos);
					std::string key = path.substr(pos+1);
					proxy->register_key(kpath, key, NSCAPI::key_string, alias, "Alias for " + alias + ". To configure this item add a section called: " + path, "", false, false);
					proxy->set_string(kpath, key, value);
					return;
				}
			}
			root_path.add_path()
				("COMMAND DEFENITION", "Command definition for: " + alias)
				;

			root_path.add_key()
				("command", sh::string_fun_key<std::string>(boost::bind(&command_object::set_command, this, _1)),
				"COMMAND", "Command to execute")

				("user", nscapi::settings_helper::string_key(&user),
				"USER", "The user to run the command as", true)

				("domain", nscapi::settings_helper::string_key(&domain),
				"DOMAIN", "The user to run the command as", true)

				("password", nscapi::settings_helper::string_key(&password),
				"PASSWORD", "The user to run the command as", true)

				("encoding", nscapi::settings_helper::string_key(&encoding),
				"ENCODING", "The encoding to parse the command as", true)

				("ignore perfdata", nscapi::settings_helper::bool_key(&ignore_perf),
				"IGNORE PERF DATA", "Do not parse performance data from the output", false)

				;

			parent::read(proxy, oneliner, is_sample);

			settings.register_all();
			settings.notify();
		}


		void set_command(std::string str) {
			command = str;
		}

	};
	typedef boost::shared_ptr<command_object> command_object_instance;

	typedef nscapi::settings_objects::object_handler<command_object> command_handler;
}

