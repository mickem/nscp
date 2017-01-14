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

namespace smtp_handler {
	namespace sh = nscapi::settings_helper;

	struct smtp_target_object : public nscapi::targets::target_object {
		typedef nscapi::targets::target_object parent;

		smtp_target_object(std::string alias, std::string path) : parent(alias, path) {
			set_property_int("timeout", 30);
			set_property_string("sender", "nscp@localhost");
			set_property_string("recipient", "nscp@localhost");
			set_property_string("template", "Hello, this is %source% reporting %message%!");
		}
		smtp_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			root_path.add_key()

				("sender", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "sender", _1), "nscp@localhost"),
					"SENDER", "Sender of email message")

				("recipient", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "recipient", _1), "nscp@localhost"),
					"RECIPIENT", "Recipient of email message")

				("template", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "template", _1), "Hello, this is %source% reporting %message%!"),
					"TEMPLATE", "Template for message data")

				;
		}
	};

	struct options_reader_impl : public client::options_reader_interface {
		virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
			return boost::make_shared<smtp_target_object>(alias, path);
		}
		virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
			return boost::make_shared<smtp_target_object>(parent, alias, path);
		}

		void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
			desc.add_options()

				("sender", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, source, "sender", _1)),
					"Length of payload (has to be same as on the server)")

				("recipient", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "recipient", _1)),
					"Length of payload (has to be same as on the server)")

				("template", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "template", _1)),
					"Do not initial an ssl handshake with the server, talk in plain text.")

				("source-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "host", _1)),
					"Source/sender host name (default is auto which means use the name of the actual host)")

				("sender-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "host", _1)),
					"Source/sender host name (default is auto which means use the name of the actual host)")

				;
		}
	};
}