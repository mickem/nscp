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
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/settings/helper.hpp>

namespace icinga_handler {
namespace sh = nscapi::settings_helper;

struct icinga_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  icinga_target_object(std::string alias, std::string path) : parent(alias, path) { set_property_int("timeout", 30); }

  icinga_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    if (oneliner) return;

    // clang-format off
    root_path.add_key()
        .add_string("username", sh::string_fun_key([this](auto value) { this->set_property_string("username", value); }),
                    "ICINGA API USER", "The username used to authenticate against the Icinga 2 REST API.")
        .add_password("password", sh::string_fun_key([this](auto value) { this->set_property_string("password", value); }),
                    "ICINGA API PASSWORD", "The password used to authenticate against the Icinga 2 REST API.")
        .add_bool("ensure objects", sh::bool_fun_key([this](auto value) { this->set_property_bool("ensure_objects", value); }),
                    "ENSURE HOST/SERVICE OBJECTS",
                    "When true the client will create missing host/service objects in Icinga 2 before submitting check results. "
                    "Objects are probed with GET first; PUT is only issued for missing objects (Icinga 2 returns HTTP 500 on duplicate creates).")
        .add_string("host template", sh::string_fun_key([this](auto value) { this->set_property_string("host_template", value); }),
                    "HOST TEMPLATE",
                    "Comma separated list of Icinga 2 templates used when auto-creating host objects (default: generic-host).")
        .add_string("service template", sh::string_fun_key([this](auto value) { this->set_property_string("service_template", value); }),
                    "SERVICE TEMPLATE",
                    "Comma separated list of Icinga 2 templates used when auto-creating service objects (default: generic-service).")
        .add_string("check command", sh::string_fun_key([this](auto value) { this->set_property_string("check_command", value); }),
                    "CHECK COMMAND",
                    "The Icinga 2 check_command to set on auto-created service objects (default: dummy).")
        .add_string("check source", sh::string_fun_key([this](auto value) { this->set_property_string("check_source", value); }),
                    "CHECK SOURCE",
                    "Override for the check_source field reported to Icinga 2. Defaults to the local hostname when empty.")
        .add_string("tls version", sh::string_fun_key([this](auto value) { this->set_property_string("tls version", value); }),
                    "TLS VERSION", "The TLS version to use 1.0, 1.1, 1.2, 1.3")
        .add_string("verify mode", sh::string_fun_key([this](auto value) { this->set_property_string("verify mode", value); }),
                    "TLS PEER VERIFY MODE",
                    "Comma separated list of options: none, peer, peer-cert, client-once, fail-if-no-cert, workarounds, single. "
                    "In general use peer-cert or none for self signed certificates.")
        .add_string("ca", sh::string_fun_key([this](auto value) { this->set_property_string("ca", value); }),
                    "CERTIFICATE AUTHORITY",
                    "Certificate authority to use when verifying certificates.")
        ;
    // clang-format on

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
    return boost::make_shared<icinga_target_object>(alias, path);
  }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return boost::make_shared<icinga_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
    // clang-format off
    desc.add_options()
    ("username", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("username", value); }),
        "The username used to authenticate against the Icinga 2 REST API.")
    ("password", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("password", value); }),
        "The password used to authenticate against the Icinga 2 REST API.")
    ("source-host", po::value<std::string>()->notifier([&source] (auto value) { source.set_string_data("host", value); }),
        "Source/sender host name (default is auto which means use the name of the actual host)")
    ("sender-host", po::value<std::string>()->notifier([&source] (auto value) { source.set_string_data("host", value); }),
        "Source/sender host name (default is auto which means use the name of the actual host)")
    ("ensure-objects", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("ensure_objects", value); }),
        "Create missing host/service objects in Icinga 2 before submitting (true/false).")
    ("host-template", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("host_template", value); }),
        "Templates used when auto-creating host objects (default: generic-host).")
    ("service-template", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("service_template", value); }),
        "Templates used when auto-creating service objects (default: generic-service).")
    ("check-command", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("check_command", value); }),
        "The check_command to set on auto-created service objects (default: dummy).")
    ("check-source", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("check_source", value); }),
        "Override for the check_source field reported to Icinga 2.")
    ("tls version", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("tls version", value); }),
        "The TLS version to use 1.0, 1.1, 1.2, 1.3")
    ("verify mode", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("verify mode", value); }),
        "Comma separated list of options: none, peer, peer-cert, client-once, fail-if-no-cert, workarounds, single. "
        "In general use peer-cert or none for self signed certificates.")
    ("ca", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("ca", value); }),
        "Certificate authority to use when verifying certificates.")
    ;
    // clang-format on
  }
};
}  // namespace icinga_handler
