#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>

#include <nscp_string.hpp>

#include <nscapi/dll_defines.hpp>

#include <net/net.hpp>

namespace sh = nscapi::settings_helper;

namespace nscapi {
	namespace targets {
		struct target_object : public nscapi::settings_objects::object_instance_interface {
			typedef nscapi::settings_objects::object_instance_interface parent;

			net::url address;
			typedef std::map<std::string,std::string> options_type;
			options_type options;

			target_object(std::string alias, std::string path) : parent(alias, path) {}

			std::string to_string() const {
				std::stringstream ss;
				ss << "{tpl: " << parent::to_string();
				ss << ", address: " << get_address();
				BOOST_FOREACH(options_type::value_type o, options) {
					ss << ", option[" << o.first << "]: " << o.second;
				}
				ss << "}";
				return ss.str();
			}
			std::string get_address() const {
				return address.to_string();
			}
			void set_address(std::string value) {
				net::url n = net::parse(value);
				address.apply(n);
			}

			virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
				//parent::read(proxy, oneliner, is_sample);
				set_address(this->value);
				nscapi::settings_helper::settings_registry settings(proxy);

				nscapi::settings_helper::path_extension root_path = settings.path(this->path);
				if (is_sample)
					root_path.set_sample();

				target_object::options_type options;
				root_path.add_path()
					(nscapi::settings_helper::string_map_path(&options),
					"TARGET DEFENITION", "Target definition for: " + this->alias,
					"TARGET", "Target definition for: " + this->alias)
					;

				root_path.add_key()

					("address", sh::string_fun_key<std::string>(boost::bind(&target_object::set_address, this, _1)),
					"TARGET ADDRESS", "Target host address")

					("host", sh::string_fun_key<std::string>(boost::bind(&target_object::set_property_string, this, "host", _1)),
					"TARGET HOST", "The target server to report results to.", true)

					("port", sh::string_fun_key<std::string>(boost::bind(&target_object::set_property_string, this, "port", _1)),
					"TARGET PORT", "The target server port", true)

					("timeout", sh::int_fun_key<int>(boost::bind(&target_object::set_property_int, this, "timeout", _1), 30),
					"TIMEOUT", "Timeout when reading/writing packets to/from sockets.")

					("retries", sh::int_fun_key<int>(boost::bind(&target_object::set_property_int, this, "retries", _1), 3),
					"RETRIES", "Number of times to retry sending.")

					;

				settings.register_all();
				settings.notify();

				BOOST_FOREACH(const target_object::options_type::value_type &kvp, options) {
					set_property_string(kvp.first, kvp.second);
				}

			}


			void add_ssl_keys(nscapi::settings_helper::path_extension root_path) {

				root_path.add_key()


					("dh", sh::path_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "dh", _1), "${certificate-path}/nrpe_dh_512.pem"),
					"DH KEY", "", true)

					("certificate", sh::path_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "certificate", _1)),
					"SSL CERTIFICATE", "", false)

					("certificate key", sh::path_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "certificate key", _1)),
					"SSL CERTIFICATE", "", true)

					("certificate format", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "certificate format", _1), "PEM"),
					"CERTIFICATE FORMAT", "", true)

					("ca", sh::path_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "ca", _1)),
					"CA", "", true)

					("allowed ciphers", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "allowed ciphers", _1), "ADH"),
					"ALLOWED CIPHERS", "A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", false)

					("verify mode", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "verify mode", _1), "none"),
					"VERIFY MODE", "", false)

					("use ssl", sh::bool_fun_key<bool>(boost::bind(&parent::set_property_bool, this, "ssl", _1), false),
					"ENABLE SSL ENCRYPTION", "This option controls if SSL should be enabled.")

					;	
			}



			virtual void translate(const std::string &key, const std::string &value) {
				if (key == "host")
					address.host = value;
				else if (key == "port")
					address.port = strEx::s::stox<unsigned int>(value);
				else
					parent::translate(key, value);
			}
			/*
			nscapi::protobuf::types::destination_container to_destination_container() const {
				nscapi::protobuf::types::destination_container ret;
				if (!tpl.alias.empty())
					ret.id = tpl.alias;
				ret.address.apply(address);
				BOOST_FOREACH(const options_type::value_type &kvp, options) {
					ret.data[kvp.first] = kvp.second;
				}
				return ret;
			}
			*/

		};
		typedef boost::optional<target_object> optional_target_object;
		typedef std::map<std::string,std::string> targets_type;

		struct handler : public nscapi::settings_objects::object_handler {
		};

	}
}

