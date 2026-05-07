/*
 * Copyright (C) 2004-2026 Michael Medin
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

#include "nsca_ng_handler.hpp"

#include <boost/make_shared.hpp>

namespace po = boost::program_options;

namespace nsca_ng_handler {
namespace sh = nscapi::settings_helper;

nsca_ng_target_object::nsca_ng_target_object(const std::string &alias, const std::string &path) : parent(alias, path) {
  set_property_int("timeout", 30);
  set_property_int("retries", 2);
  set_property_string("port", "5668");
  set_property_bool("use psk", true);
  set_property_bool("host check", false);
}

nsca_ng_target_object::nsca_ng_target_object(const nscapi::settings_objects::object_instance &other, const std::string &alias, const std::string &path)
    : parent(other, alias, path) {}

void nsca_ng_target_object::read(const nscapi::settings_helper::settings_impl_interface_ptr proxy, const bool oneliner, const bool is_sample) {
  parent::read(proxy, oneliner, is_sample);

  nscapi::settings_helper::settings_registry settings(proxy);
  nscapi::settings_helper::path_extension root_path = settings.path(get_path());
  if (is_sample) root_path.set_sample();

  add_ssl_keys(root_path);

  root_path
      .add_key()

      .add_password("password", sh::string_fun_key([this](const auto &value) { this->set_property_string("password", value); }, ""), "PASSWORD",
                    "The password used for NSCA-NG PSK authentication. Must match the server configuration.")

      .add_string("identity", sh::string_fun_key([this](const auto &value) { this->set_property_string("identity", value); }, ""), "IDENTITY",
                  "The PSK identity sent to the server. Defaults to the sender hostname when empty.")

      .add_bool("use psk", sh::bool_fun_key([this](auto value) { this->set_property_bool("use psk", value); }, true), "USE PSK",
                "Use TLS-PSK for authentication. When false, use certificate-based TLS.", true)

      .add_bool("insecure", sh::bool_fun_key([this](auto value) { this->set_property_bool("insecure", value); }, false), "INSECURE",
                "When true, allow TLS connections that do not authenticate the server (no PSK and no peer-cert verification). "
                "Off by default; enabling this disables protection against man-in-the-middle attacks.",
                true)

      .add_int("max output length", sh::int_fun_key([this](auto value) { this->set_property_int("max output length", value); }, 65536), "MAX OUTPUT LENGTH",
               "Maximum number of bytes from the plugin output to forward to the server. Default 65536 (64 KiB).", true)

      .add_bool("host check", sh::bool_fun_key([this](auto value) { this->set_property_bool("host check", value); }, false), "HOST CHECK",
                "Submit every result on this target as a Nagios host check (PROCESS_HOST_CHECK_RESULT) instead of a service check. "
                "Default false. The legacy alias 'host_check' is also still honoured for backwards compatibility.",
                true);

  settings.register_all();
  settings.notify();
}

// =====================================================================
// options_reader_impl
// =====================================================================

nscapi::settings_objects::object_instance options_reader_impl::create(std::string alias, std::string path) {
  return boost::make_shared<nsca_ng_target_object>(alias, path);
}

nscapi::settings_objects::object_instance options_reader_impl::clone(nscapi::settings_objects::object_instance parent_obj, const std::string alias,
                                                                     const std::string path) {
  return boost::make_shared<nsca_ng_target_object>(parent_obj, alias, path);
}

void options_reader_impl::process(boost::program_options::options_description &desc, client::destination_container &source,
                                  client::destination_container &data) {
  add_ssl_options(desc, data);

  // clang-format off
  desc.add_options()
    ("password", po::value<std::string>()->notifier([&data](const auto& value) { data.set_string_data("password", value); }),
      "The PSK password (must match the NSCA-NG server configuration)")
    ("identity", po::value<std::string>()->notifier([&data](const auto& value) { data.set_string_data("identity", value); }),
      "PSK identity string (defaults to hostname when empty)")
    ("hostname", po::value<std::string>()->notifier([&source](const auto& value) { source.set_string_data("host", value); }),
      "Host name to report to the NSCA-NG server")
    ("no-psk", po::bool_switch()->notifier([&data](bool v) { if (v) data.set_bool_data("use psk", false); }),
      "Disable PSK and use certificate-based TLS authentication instead")
    ("insecure", po::bool_switch()->notifier([&data](bool v) { if (v) data.set_bool_data("insecure", true); }),
      "Allow TLS connections without PSK and without peer-cert verification. Disables MITM protection.")
    ("host-check", po::bool_switch()->notifier([&data](bool v) { if (v) data.set_bool_data("host check", true); }),
      "Submit every result as a Nagios host check (PROCESS_HOST_CHECK_RESULT) instead of a service check.")
    ("max-output-length", po::value<int>()->notifier([&data](int v) { data.set_int_data("max output length", v); }),
      "Maximum bytes of plugin output forwarded over the wire (default 65536)")
  ;
  // clang-format on
}

}  // namespace nsca_ng_handler
