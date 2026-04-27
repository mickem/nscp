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

namespace nrdp_handler {
namespace sh = nscapi::settings_helper;

struct nrdp_target_object : nscapi::targets::target_object {
  typedef target_object parent;

  nrdp_target_object(const std::string& alias, const std::string& path) : parent(alias, path) { set_property_int("timeout", 30); }

  nrdp_target_object(const nscapi::settings_objects::object_instance& other, const std::string& alias, const std::string& path) : parent(other, alias, path) {}

  void read(const nscapi::settings_helper::settings_impl_interface_ptr proxy, const bool oneliner, bool is_sample) override {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    if (oneliner) return;

    root_path.add_key()
        .add_string("key", sh::string_fun_key([this](const auto& value) { this->set_property_string("token", value); }), "SECURITY TOKEN", "The security token")
        .add_password("password", sh::string_fun_key([this](const auto& value) { this->set_property_string("token", value); }), "SECURITY TOKEN", "The security token")
        .add_password("token", sh::string_fun_key([this](const auto& value) { this->set_property_string("token", value); }), "SECURITY TOKEN", "The security token")
        .add_string("tls version", sh::string_fun_key([this](const auto& value) { this->set_property_string("tls version", value); }), "Tls version",
                    "The tls version to use 1.0, 1.1, 1.2, 1.3")
        .add_string("verify mode", sh::string_fun_key([this](const auto& value) { this->set_property_string("verify mode", value); }), "TLS peer verify mode",
                    "Coma separated list o9f option none, peer, peer-cert, client-once, fail-if-no-cert, workarounds, single., In general use peer-cert or "
                    "none for self signed certificates.")
        .add_string("ca", sh::string_fun_key([this](const auto& value) { this->set_property_string("ca", value); }), "Certificate Authority",
                    "Certificate authority to use when verifying certificates.");

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : client::options_reader_interface {
  nscapi::settings_objects::object_instance create(std::string alias, std::string path) override { return boost::make_shared<nrdp_target_object>(alias, path); }
  nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) override {
    return boost::make_shared<nrdp_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) override {
    // clang-format off
    desc.add_options()
    ("key", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("token", value); }),
    "The security token")
    ("password", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("token", value); }),
    "The security token")
    ("source-host", po::value<std::string>()->notifier([&source] (const auto& value) { source.set_string_data("host", value); }),
    "Source/sender host name (default is auto which means use the name of the actual host)")
    ("sender-host", po::value<std::string>()->notifier([&source] (const auto& value) { source.set_string_data("host", value); }),
    "Source/sender host name (default is auto which means use the name of the actual host)")
    ("token", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("token", value); }),
    "The security token")
    ("tls-version", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("tls version", value); }),
      "The tls version to use 1.0, 1.1, 1.2, 1.3")
    ("tls version", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("tls version", value); }),
      "Legacy alias for --tls-version (kept for backwards compatibility).")
    ("verify", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("verify mode", value); }),
      "Coma separated list of option none, peer, peer-cert, client-once, fail-if-no-cert, workarounds, single. In general use peer-cert or none for self signed certificates.")
    ("verify-mode", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("verify mode", value); }),
      "Alias for --verify.")
    ("verify mode", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("verify mode", value); }),
      "Legacy alias for --verify (kept for backwards compatibility).")
    ("ca", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("ca", value); }),
      "Certificate authority to use when verifying certificates.")
    ;
    // clang-format on
  }
};
}  // namespace nrdp_handler