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

#include <nscapi/nscapi_targets.hpp>

#include <nscapi/nscapi_settings_helper.hpp>

#include <net/net.hpp>

#include <boost/bind/bind.hpp>

namespace sh = nscapi::settings_helper;
namespace ph = boost::placeholders;

void nscapi::targets::target_object::read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool, bool is_sample) {
  set_address(this->get_value());
  nscapi::settings_helper::settings_registry settings(proxy);

  nscapi::settings_helper::path_extension root_path = settings.path(get_path());
  if (is_sample) root_path.set_sample();

  // clang-format off
	root_path.add_path()
		("TARGET", "Target definition for: " + this->get_alias())
		;

	root_path.add_key()

		("address", sh::string_fun_key(boost::bind(&target_object::set_address, this, ph::_1)),
		"TARGET ADDRESS", "Target host address")

		("host", sh::string_fun_key(boost::bind(&target_object::set_property_string, this, "host", ph::_1)),
		"TARGET HOST", "The target server to report results to.", true)

		("port", sh::string_fun_key(boost::bind(&target_object::set_property_string, this, "port", ph::_1)),
		"TARGET PORT", "The target server port", true)

		("timeout", sh::int_fun_key(boost::bind(&target_object::set_property_int, this, "timeout", ph::_1), 30),
		"TIMEOUT", "Timeout when reading/writing packets to/from sockets.")

		("retries", sh::int_fun_key(boost::bind(&target_object::set_property_int, this, "retries", ph::_1), 3),
		"RETRIES", "Number of times to retry sending.")

		;
  // clang-format on

  settings.register_all();
  settings.notify();
}

void nscapi::targets::target_object::add_ssl_keys(nscapi::settings_helper::path_extension root_path) {
  // clang-format off
	root_path.add_key()

		("dh", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "dh", ph::_1)),
		"DH KEY", "", true)

		("certificate", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "certificate", ph::_1)),
		"SSL CERTIFICATE", "", false)

		("certificate key", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "certificate key", ph::_1)),
		"SSL CERTIFICATE", "", true)

		("certificate format", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "certificate format", ph::_1)),
		"CERTIFICATE FORMAT", "", true)

		("ca", sh::path_fun_key(boost::bind(&parent::set_property_string, this, "ca", ph::_1)),
		"CA", "", true)

		("allowed ciphers", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "allowed ciphers", ph::_1)),
		"ALLOWED CIPHERS", "A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", false)

		("verify mode", sh::string_fun_key(boost::bind(&parent::set_property_string, this, "verify mode", ph::_1)),
		"VERIFY MODE", "", false)

		("use ssl", sh::bool_fun_key(boost::bind(&parent::set_property_bool, this, "ssl", ph::_1)),
		"ENABLE SSL ENCRYPTION", "This option controls if SSL should be enabled.")

		;
  // clang-format on
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
