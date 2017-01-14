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

#include <net/net.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>

#include <map>
#include <string>

namespace sh = nscapi::settings_helper;

namespace nscapi {
	namespace targets {
		struct target_object : public nscapi::settings_objects::object_instance_interface {
			typedef nscapi::settings_objects::object_instance_interface parent;

			target_object(std::string alias, std::string path) : parent(alias, path) {}
			target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

			std::string to_string() const {
				std::stringstream ss;
				ss << "{tpl: " << parent::to_string()
					<< "}";
				return ss.str();
			}
			void set_address(std::string value) {
				set_property_string("address", value);
			}

			virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
				set_address(this->get_value());
				nscapi::settings_helper::settings_registry settings(proxy);

				nscapi::settings_helper::path_extension root_path = settings.path(get_path());
				if (is_sample)
					root_path.set_sample();

				root_path.add_path()
					("TARGET", "Target definition for: " + this->get_alias())
					;

				root_path.add_key()

					("address", sh::string_fun_key(boost::bind(&target_object::set_address, this, _1)),
						"TARGET ADDRESS", "Target host address")

					("host", sh::string_fun_key(boost::bind(&target_object::set_property_string, this, "host", _1)),
						"TARGET HOST", "The target server to report results to.", true)

					("port", sh::string_fun_key(boost::bind(&target_object::set_property_string, this, "port", _1)),
						"TARGET PORT", "The target server port", true)

					("timeout", sh::int_fun_key(boost::bind(&target_object::set_property_int, this, "timeout", _1), 30),
						"TIMEOUT", "Timeout when reading/writing packets to/from sockets.")

					("retries", sh::int_fun_key(boost::bind(&target_object::set_property_int, this, "retries", _1), 3),
						"RETRIES", "Number of times to retry sending.")

					;

				settings.register_all();
				settings.notify();
			}

			void add_ssl_keys(nscapi::settings_helper::path_extension root_path) {
				root_path.add_key()

					("dh", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "dh", _1)),
						"DH KEY", "", true)

					("certificate", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "certificate", _1)),
						"SSL CERTIFICATE", "", false)

					("certificate key", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "certificate key", _1)),
						"SSL CERTIFICATE", "", true)

					("certificate format", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "certificate format", _1)),
						"CERTIFICATE FORMAT", "", true)

					("ca", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "ca", _1)),
						"CA", "", true)

					("allowed ciphers", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "allowed ciphers", _1)),
						"ALLOWED CIPHERS", "A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", false)

					("verify mode", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "verify mode", _1)),
						"VERIFY MODE", "", false)

					("use ssl", sh::bool_fun_key(boost::bind(&parent::set_property_bool, this, "ssl", _1)),
						"ENABLE SSL ENCRYPTION", "This option controls if SSL should be enabled.")

					;
			}

			virtual void translate(const std::string &key, const std::string &value) {
				if (key == "host") {
					net::url n = net::parse(get_property_string("address"));
					n.host = value;
					set_property_string("address", n.to_string());
				} else if (key == "port") {
					net::url n = net::parse(get_property_string("address"));
					n.port = str::stox<unsigned int>(value);
					set_property_string("address", n.to_string());
				} else
					parent::translate(key, value);
			}
		};
		typedef boost::optional<target_object> optional_target_object;
		typedef std::map<std::string, std::string> targets_type;

		typedef nscapi::settings_objects::object_handler<target_object> handler;
	}
}