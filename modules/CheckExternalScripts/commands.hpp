/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>

#include <boost/algorithm/string/case_conv.hpp>

#include <string>


namespace sh = nscapi::settings_helper;

namespace commands {
	struct command_object : public nscapi::settings_objects::object_instance_interface {
		typedef nscapi::settings_objects::object_instance_interface parent;

		command_object(std::string alias, std::string path)
			: parent(alias, path)
			, display(false)
			, ignore_perf(false)
			, no_fork(true)
		{}

		std::string encoding;
		std::string command;
		std::string user;
		std::string domain;
		std::string password;
		std::string session;
		bool display;
		bool ignore_perf;
		bool no_fork;

		std::string to_string() const {
			std::stringstream ss;
			ss << get_alias() << "[" << get_alias() << "] = "
				<< "{tpl: " << parent::to_string();
			if (!user.empty()) {
				ss << ", user: " << user
					<< ", domain: " << domain
					<< ", password: " << password
					<< ", session: " << session
					<< ", display: " << display
					<< ", no_fork: " << no_fork;
			}
			ss << "}";
			return ss.str();
		}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);
			set_alias(boost::algorithm::to_lower_copy(get_alias()));
			set_command(get_value());

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			if (!oneliner) {
				root_path.add_path()
					("script: " + get_alias(), "The configuration section for the  " + get_alias() + " script.")
					;

				root_path.add_key()
					("command", sh::string_fun_key(boost::bind(&command_object::set_command, this, _1)),
						"COMMAND", "Command to execute")

					("user", nscapi::settings_helper::string_key(&user),
						"USER", "The user to run the command as", true)

					("domain", nscapi::settings_helper::string_key(&domain),
						"DOMAIN", "The user to run the command as", true)

					("password", nscapi::settings_helper::string_key(&password),
						"PASSWORD", "The user to run the command as", true)

					("session", nscapi::settings_helper::string_key(&session),
						"SESSION", "Session you want to invoke the client in either the number of current for the one with a UI", true)

					("display", nscapi::settings_helper::bool_key(&display),
						"DISPLAY", "Set to true if you want to display the resulting window or not", true)

					("encoding", nscapi::settings_helper::string_key(&encoding),
						"ENCODING", "The encoding to parse the command as", true)

					("ignore perfdata", nscapi::settings_helper::bool_key(&ignore_perf),
						"IGNORE PERF DATA", "Do not parse performance data from the output", false)

					("capture output", nscapi::settings_helper::bool_key(&no_fork),
						"CAPTURE OUTPUT", "This should be set to false if you want to run commands which never terminates (i.e. relinquish control from NSClient++). The effect of this is that the command output will not be captured. The main use is to protect from socket reuse issues", true)

					;

				settings.register_all();
				settings.notify();
			}
		}

		void set_command(std::string str) {
			command = str;
		}
	};
	typedef boost::shared_ptr<command_object> command_object_instance;

	typedef nscapi::settings_objects::object_handler<command_object> command_handler;
}