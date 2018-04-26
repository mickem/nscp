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

namespace web_server {
	namespace sh = nscapi::settings_helper;

	struct user_config_object : public nscapi::simple_settings_objects::object_instance_interface {
		typedef nscapi::simple_settings_objects::object_instance_interface parent;

		std::string password;
		std::string role;

		user_config_object(std::string alias, std::string path) : parent(alias, path) {
		}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool is_sample) {
			parent::read(proxy, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			root_path.add_key()

				("password", sh::string_key(&password),
					"PASSWORD", "The password to use.")

				("role", sh::string_key(&role),
					"ROLE", "The role which will grant access to this user")

				;

			settings.register_all();
			settings.notify();
		}
	};
	typedef boost::shared_ptr<user_config_object> user_config_instance;

	typedef nscapi::simple_settings_objects::object_handler<user_config_object> user_config;


}