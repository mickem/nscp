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

#include <boost/make_shared.hpp>
#include <nsca/client/nsca_client_protocol.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

namespace nsca_handler {
namespace sh = nscapi::settings_helper;

struct nsca_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  nsca_target_object(const std::string &alias, const std::string &path) : parent(alias, path) {
    set_property_int("timeout", 30);
    set_property_int("retries", 3);
    set_property_string("encryption", "aes256");
    set_property_int("payload length", 512);
    set_property_string("port", "5667");
    set_property_int("time offset", 0);
  }
  nsca_target_object(const nscapi::settings_objects::object_instance &other, const std::string &alias, const std::string &path) : parent(other, alias, path) {}

  void read(const nscapi::settings_helper::settings_impl_interface_ptr proxy, const bool oneliner, const bool is_sample) override {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    add_ssl_keys(root_path);

    root_path
        .add_key()

        .add_int("payload length", sh::int_fun_key([this](auto value) { this->set_property_int("payload length", value); }, 512), "PAYLOAD LENGTH",
                 "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use "
                 "the same value for it to work.",
                 true)

        .add_string("encryption", sh::string_fun_key([this](auto value) { this->set_property_string("encryption", value); }, "aes"), "ENCRYPTION",
                    std::string("Name of encryption algorithm to use.\nHas to be the same as your server i using or it wont work at all."
                                "This is also independent of SSL and generally used instead of SSL.\nAvailable encryption algorithms are:\n") +
                        nscp::encryption::helpers::get_crypto_string("\n"))

        .add_password("password", sh::string_fun_key([this](auto value) { this->set_property_string("password", value); }, ""), "PASSWORD",
                      "The password to use. Again has to be the same as the server or it wont work at all.")

        .add_string("encoding", sh::string_fun_key([this](auto value) { this->set_property_string("encoding", value); }, ""), "ENCODING", "", true)

        .add_string("time offset", sh::string_fun_key([this](auto value) { this->set_property_string("delay", value); }, "0"), "TIME OFFSET", "Time offset.",
                    true);

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  nscapi::settings_objects::object_instance create(std::string alias, std::string path) override { return boost::make_shared<nsca_target_object>(alias, path); }
  nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) override {
    return boost::make_shared<nsca_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) override {
    add_ssl_options(desc, data);

    // clang-format off
  desc.add_options()
    ("encryption,e", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("encryption", value); }),
	    (std::string("Name of encryption algorithm to use.\nHas to be the same as your server i using or it wont work at all."
		    "This is also independent of SSL and generally used instead of SSL.\nAvailable encryption algorithms are:\n") + nscp::encryption::helpers::get_crypto_string("\n")).c_str())
    ("payload-length,l", po::value<unsigned int>()->notifier([&data] (auto value) { data.set_int_data("payload length", value); }),
	    "Length of payload (has to be same as on the server)")
    ("buffer-length", po::value<unsigned int>()->notifier([&data] (auto value) { data.set_int_data("payload length", value); }),
	    "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.")
    ("password", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("password", value); }),
	    "Password")
    ("hostname", po::value<std::string>()->notifier([&source] (auto value) { source.set_string_data("host", value); }),
      "Host name to report")
    ("time-offset", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("time offset", value); }),
	    "")
  ;
    // clang-format on
  }
};
}  // namespace nsca_handler