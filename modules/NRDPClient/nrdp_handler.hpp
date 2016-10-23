/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <utils.h>
#include <strEx.h>

#include <socket/client.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <boost/make_shared.hpp>

namespace nrdp_handler {
	namespace sh = nscapi::settings_helper;

	struct nrdp_target_object : public nscapi::targets::target_object {
		typedef nscapi::targets::target_object parent;

		nrdp_target_object(std::string alias, std::string path) : parent(alias, path) {
			set_property_int("timeout", 30);
		}

		nrdp_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			if (oneliner)
				return;

			root_path.add_key()

				("key", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "token", _1)),
					"SECURITY TOKEN", "The security token")

				("password", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "token", _1)),
					"SECURITY TOKEN", "The security token")

				("token", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "token", _1)),
					"SECURITY TOKEN", "The security token")

				;

			settings.register_all();
			settings.notify();

		}
	};

	struct options_reader_impl : public client::options_reader_interface {
		virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
			return boost::make_shared<nrdp_target_object>(alias, path);
		}
		virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
			return boost::make_shared<nrdp_target_object>(parent, alias, path);
		}

		void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
			desc.add_options()

				("key", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "token", _1)),
					"The security token")

				("password", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "token", _1)),
					"The security token")

				("source-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "host", _1)),
					"Source/sender host name (default is auto which means use the name of the actual host)")

				("sender-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "host", _1)),
					"Source/sender host name (default is auto which means use the name of the actual host)")

				("token", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "token", _1)),
					"The security token")

				;
		}
	};
}