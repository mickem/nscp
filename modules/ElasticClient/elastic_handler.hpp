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

#include <utils.h>
#include <str/xtos.hpp>

#include <socket/client.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <boost/make_shared.hpp>

namespace elastic_handler {
	namespace sh = nscapi::settings_helper;

	struct elastic_target_object : public nscapi::targets::target_object {
		typedef nscapi::targets::target_object parent;

		elastic_target_object(std::string alias, std::string path) : parent(alias, path) {
			set_property_bool("send perfdata", true);
			set_property_bool("send status", true);
			set_property_int("timeout", 30);
			set_property_string("perf path", "system.${hostname}.${check_alias}.${perf_alias}");
			set_property_string("status path", "system.${hostname}.${check_alias}.status");
		}
		elastic_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			if (is_default()) {

				root_path.add_key()

					("path", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "perf path", _1), "system.${hostname}.${check_alias}.${perf_alias}"),
						"PATH FOR METRICS", "Path mapping for metrics")

					("status path", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "status path", _1), "system.${hostname}.${check_alias}.status"),
						"PATH FOR STATUS", "Path mapping for status")

					("send perfdata", sh::bool_fun_key(boost::bind(&parent::set_property_bool, this, "send perfdata", _1), true),
						"SEND PERF DATA", "Send performance data to this server")

					("send status", sh::bool_fun_key(boost::bind(&parent::set_property_bool, this, "send status", _1), true),
						"SEND STATUS", "Send status data to this server")

					;
			} else {
				root_path.add_key()

					("path", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "perf path", _1)),
						"PATH FOR METRICS", "Path mapping for metrics")

					("status path", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "status path", _1)),
						"PATH FOR STATUS", "Path mapping for status")

					("send perfdata", sh::bool_fun_key(boost::bind(&parent::set_property_bool, this, "send perfdata", _1)),
						"SEND PERF DATA", "Send performance data to this server")

					("send status", sh::bool_fun_key(boost::bind(&parent::set_property_bool, this, "send status", _1)),
						"SEND STATUS", "Send status data to this server")

					;
			}
			settings.register_all();
			settings.notify();
		}
	};

	struct options_reader_impl : public client::options_reader_interface {
		virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
			return boost::make_shared<elastic_target_object>(alias, path);
		}
		virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
			return boost::make_shared<elastic_target_object>(parent, alias, path);
		}

		void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
			desc.add_options()
				("path", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "path", _1)),
					"")

				;
		}
	};
}