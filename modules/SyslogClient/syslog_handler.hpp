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

#include <str/xtos.hpp>

#include <socket/client.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <boost/make_shared.hpp>

namespace syslog_handler {
	namespace sh = nscapi::settings_helper;

	struct syslog_target_object : public nscapi::targets::target_object {
		typedef nscapi::targets::target_object parent;

		syslog_target_object(std::string alias, std::string path) : parent(alias, path) {
			set_property_int("timeout", 30);
			set_property_string("path", "/nsclient++");
			set_property_string("severity", "error");
			set_property_string("facility", "kernel");
			set_property_string("tag syntax", "NSCA");
			set_property_string("message syntax", "%message%");
			set_property_string("ok severity", "informational");
			set_property_string("warning severity", "warning");
			set_property_string("critical severity", "critical");
			set_property_string("unknown severity", "emergency");
		}
		syslog_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			root_path.add_key()

				("severity", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "severity", _1), "error"),
					"TODO", "")

				("facility", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "facility", _1), "kernel"),
					"TODO", "")

				("tag_syntax", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "tag syntax", _1), "NSCA"),
					"TODO", "")

				("message_syntax", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "message syntax", _1), "%message%"),
					"TODO", "")

				("ok severity", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "ok severity", _1), "informational"),
					"TODO", "")

				("warning severity", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "warning severity", _1), "warning"),
					"TODO", "")

				("critical severity", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "critical severity", _1), "critical"),
					"TODO", "")

				("unknown severity", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "unknown severity", _1), "emergency"),
					"TODO", "")
				;
		}
	};

	struct options_reader_impl : public client::options_reader_interface {
		virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
			return boost::make_shared<syslog_target_object>(alias, path);
		}
		virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
			return boost::make_shared<syslog_target_object>(parent, alias, path);
		}

		void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
			desc.add_options()
				("path", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "path", _1)),
					"")
				("severity,s", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "severity", _1)),
					"Severity of error message")

				("unknown-severity", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "unknown_severity", _1)),
					"Severity of error message")

				("ok-severity", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "ok_severity", _1)),
					"Severity of error message")

				("warning-severity", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "warning_severity", _1)),
					"Severity of error message")

				("critical-severity", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "critical_severity", _1)),
					"Severity of error message")

				("facility,f", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "facility", _1)),
					"Facility of error message")

				("tag template", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "tag template", _1)),
					"Tag template (TODO)")

				("message template", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "message template", _1)),
					"Message template (TODO)")

				;
		}
	};
}