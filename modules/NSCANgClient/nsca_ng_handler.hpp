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
#include <nscapi/settings/helper.hpp>

namespace po = boost::program_options;

namespace nsca_ng_handler {
namespace sh = nscapi::settings_helper;

struct nsca_ng_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  nsca_ng_target_object(const std::string &alias, const std::string &path) : parent(alias, path) {
    set_property_int("timeout", 30);
    set_property_int("retries", 3);
    set_property_string("port", "5668");
    set_property_bool("use psk", true);
  }

  nsca_ng_target_object(const nscapi::settings_objects::object_instance &other, const std::string &alias, const std::string &path)
      : parent(other, alias, path) {}

  void read(const nscapi::settings_helper::settings_impl_interface_ptr proxy, const bool oneliner, const bool is_sample) override {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);
    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    add_ssl_keys(root_path);

    root_path.add_key()

        .add_password("password", sh::string_fun_key([this](auto value) { this->set_property_string("password", value); }, ""), "PASSWORD",
                      "The password used for NSCA-NG PSK authentication. Must match the server configuration.")

        .add_string("identity", sh::string_fun_key([this](auto value) { this->set_property_string("identity", value); }, ""), "IDENTITY",
                    "The PSK identity sent to the server. Defaults to the sender hostname when empty.")

        .add_bool("use psk", sh::bool_fun_key([this](auto value) { this->set_property_bool("use psk", value); }, true), "USE PSK",
                  "Use TLS-PSK for authentication. When false, use certificate-based TLS.", true)

        .add_string("ciphers", sh::string_fun_key([this](auto value) { this->set_property_string("ciphers", value); }, ""), "TLS CIPHERS",
                    "Comma-separated list of TLS cipher suites to use for PSK. Defaults to PSK-AES256-CBC-SHA256.", true);

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  nscapi::settings_objects::object_instance create(std::string alias, std::string path) override {
    return boost::make_shared<nsca_ng_target_object>(alias, path);
  }

  nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent_obj, const std::string alias, const std::string path) override {
    return boost::make_shared<nsca_ng_target_object>(parent_obj, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) override {
    add_ssl_options(desc, data);

    // clang-format off
    desc.add_options()
      ("password", po::value<std::string>()->notifier([&data](auto value) { data.set_string_data("password", value); }),
        "The PSK password (must match the NSCA-NG server configuration)")
      ("identity", po::value<std::string>()->notifier([&data](auto value) { data.set_string_data("identity", value); }),
        "PSK identity string (defaults to hostname when empty)")
      ("hostname", po::value<std::string>()->notifier([&source](auto value) { source.set_string_data("host", value); }),
        "Host name to report to the NSCA-NG server")
      ("no-psk", po::bool_switch()->notifier([&data](bool v) { if (v) data.set_bool_data("use psk", false); }),
        "Disable PSK and use certificate-based TLS authentication instead")
    ;
    // clang-format on
  }
};

}  // namespace nsca_ng_handler
