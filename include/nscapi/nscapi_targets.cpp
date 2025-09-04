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

#include <net/net.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_targets.hpp>

namespace sh = nscapi::settings_helper;

void nscapi::targets::target_object::read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool, bool is_sample) {
  set_address(this->get_value());
  nscapi::settings_helper::settings_registry settings(proxy);

  nscapi::settings_helper::path_extension root_path = settings.path(get_path());
  if (is_sample) root_path.set_sample();

  root_path.add_path()("TARGET", "Target definition for: " + this->get_alias());

  root_path.add_key()
      .add_string("address", sh::string_fun_key([this](auto key) { this->set_address(key); }), "TARGET ADDRESS", "Target host address")
      .add_string("host", sh::string_fun_key([this](auto key) { this->set_property_string("host", key); }), "TARGET HOST",
                  "The target server to report results to.", true)
      .add_string("port", sh::string_fun_key([this](auto key) { this->set_property_string("port", key); }), "TARGET PORT", "The target server port", true)
      .add_string("timeout", sh::int_fun_key([this](auto key) { this->set_property_int("timeout", key); }, 30), "TIMEOUT",
                  "Timeout (in seconds) when reading/writing packets to/from sockets.")
      .add_int("retries", sh::int_fun_key([this](auto key) { this->set_property_int("retries", key); }, 3), "RETRIES", "Number of times to retry sending.");

  settings.register_all();
  settings.notify();
}

void nscapi::targets::target_object::add_ssl_keys(nscapi::settings_helper::path_extension root_path) {
  root_path.add_key()
      .add_string("dh", sh::path_fun_key([this](auto key) { this->set_property_string("dh", key); }), "DH KEY", "", true)
      .add_string("certificate", sh::path_fun_key([this](auto key) { this->set_property_string("certificate", key); }), "SSL CERTIFICATE", "", false)
      .add_string("certificate key", sh::path_fun_key([this](auto key) { this->set_property_string("certificate key", key); }), "SSL CERTIFICATE", "", true)
      .add_string("certificate format", sh::string_fun_key([this](auto key) { this->set_property_string("certificate format", key); }), "CERTIFICATE FORMAT",
                  "", true)
      .add_string("ca", sh::path_fun_key([this](auto key) { this->set_property_string("ca", key); }), "CA", "", true)
      .add_string("allowed ciphers", sh::string_fun_key([this](auto key) { this->set_property_string("allowed ciphers", key); }), "ALLOWED CIPHERS",
                  "A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", false)
      .add_string("verify mode", sh::string_fun_key([this](auto key) { this->set_property_string("verify mode", key); }), "VERIFY MODE", "", false)
      .add_bool("use ssl", sh::bool_fun_key([this](auto key) { this->set_property_bool("ssl", key); }), "ENABLE SSL ENCRYPTION",
                "This option controls if SSL should be enabled.");
}

std::string nscapi::targets::target_object::to_string() const {
  std::stringstream ss;
  ss << "{tpl: " << parent::to_string() << "}";
  return ss.str();
}

void nscapi::targets::target_object::translate(const std::string &key, const std::string &new_value) {
  if (key == "host") {
    net::url n = net::parse(get_property_string("address"));
    n.host = new_value;
    set_property_string("address", n.to_string());
  } else if (key == "port") {
    net::url n = net::parse(get_property_string("address"));
    n.port = str::stox<unsigned int>(new_value, 0);
    set_property_string("address", n.to_string());
  } else
    parent::translate(key, new_value);
}
