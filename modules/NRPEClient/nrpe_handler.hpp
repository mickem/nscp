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

#include <nsca/nsca_packet.hpp>

#include <nsca/client/nsca_client_protocol.hpp>

#include <nscapi/nscapi_settings_helper.hpp>

#include <boost/make_shared.hpp>

namespace nrpe_handler {
	namespace sh = nscapi::settings_helper;

	struct nrpe_target_object : public nscapi::targets::target_object {
		typedef nscapi::targets::target_object parent;

		nrpe_target_object(std::string alias, std::string path) : parent(alias, path) {
			set_property_int("timeout", 30);
			set_property_string("certificate", "${certificate-path}/certificate.pem");
			set_property_string("certificate key", "");
			set_property_string("certificate format", "PEM");
			set_property_string("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			set_property_string("verify mode", "none");
			set_property_bool("insecure", false);
			set_property_bool("ssl", true);
			set_property_int("payload length", 1024);
		}

		nrpe_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			root_path.add_key()

				("insecure", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "insecure", _1)),
					"Insecure legacy mode", "Use insecure legacy mode to connect to old NRPE server", false)

				("payload length", sh::int_fun_key(boost::bind(&parent::set_property_int, this, "payload length", _1)),
					"PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.")
				;
			settings.register_all();
			settings.notify();
			settings.clear();

			add_ssl_keys(root_path);

			settings.register_all();
			settings.notify();
		}

		virtual void translate(const std::string &key, const std::string &value) {
			if (key == "insecure") {
				if (value == "true" && get_property_string("insecure", "false") != value) {
					set_property_string("certificate", "");
					set_property_string("certificate key", "");
					set_property_string("allowed ciphers", "ADH");
					set_property_string("verify mode", "none");
					set_property_bool("ssl", true);
				}
			}
			parent::translate(key, value);
		}
	};

	struct options_reader_impl : public client::options_reader_interface {
		virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
			return boost::make_shared<nrpe_target_object>(alias, path);
		}
		virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
			return boost::make_shared<nrpe_target_object>(parent, alias, path);
		}

		void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &target) {

			namespace po = boost::program_options;

			add_ssl_options(desc, target);

			desc.add_options()

				("insecure", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&client::destination_container::set_bool_data, &target, "insecure", _1)),
					"Use insecure legacy mode")

				("payload-length,l", po::value<unsigned int>()->notifier(boost::bind(&client::destination_container::set_int_data, &target, "payload length", _1)),
					"Length of payload (has to be same as on the server)")

				("buffer-length", po::value<unsigned int>()->notifier(boost::bind(&client::destination_container::set_int_data, &target, "payload length", _1)),
					"Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.")
				;
		}
	};
}