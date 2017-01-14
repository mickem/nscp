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

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>

#include <nscapi/dll_defines.hpp>

#include <boost/shared_ptr.hpp>

#include <string>


namespace nscapi {
	namespace targets {
		NSCAPI_EXPORT struct target_object : public nscapi::settings_objects::object_instance_interface {
			typedef nscapi::settings_objects::object_instance_interface parent;

			NSCAPI_EXPORT target_object(std::string alias, std::string path) : parent(alias, path) {}
			NSCAPI_EXPORT target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

			NSCAPI_EXPORT std::string to_string() const;
			NSCAPI_EXPORT void set_address(std::string value) {
				set_property_string("address", value);
			}

			NSCAPI_EXPORT virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);

			NSCAPI_EXPORT void add_ssl_keys(nscapi::settings_helper::path_extension root_path);

			NSCAPI_EXPORT virtual void translate(const std::string &key, const std::string &value);
		};
	}
}
