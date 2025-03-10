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

#include <collectd/collectd_packet.hpp>

#include <socket/client.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <boost/make_shared.hpp>

#include "collectd_client.hpp"

namespace collectd_handler {
namespace sh = nscapi::settings_helper;

struct collectd_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  collectd_target_object(std::string alias, std::string path) : parent(alias, path) {
    set_property_string("port", "25826");
    set_property_string("host", "239.192.74.66");
  }
  collectd_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    // add_ssl_keys(root_path);

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
    return boost::make_shared<collectd_target_object>(alias, path);
  }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return boost::make_shared<collectd_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
    // add_ssl_options(desc, data);

    // clang-format off
			desc.add_options()

				("payload-length,l", po::value<unsigned int>()->notifier(boost::bind(&client::destination_container::set_int_data, &data, "payload length", boost::placeholders::_1)),
					"Length of payload (has to be same as on the server)")

				("buffer-length", po::value<unsigned int>()->notifier(boost::bind(&client::destination_container::set_int_data, &data, "payload length", boost::placeholders::_1)),
					"Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.")

				("password", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "password", boost::placeholders::_1)),
					"Password")

				("time-offset", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &data, "time offset", boost::placeholders::_1)),
					"")
				;
    // clang-format on
  }
};
}  // namespace collectd_handler