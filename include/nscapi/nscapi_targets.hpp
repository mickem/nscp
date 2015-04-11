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

			virtual void init_default() {
			}

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
			void set_host(std::string value) {
				address.host = value;
			}
			void set_port(std::string value) {
				address.port = strEx::s::stox<unsigned int>(value);
			}
			virtual void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, bool is_sample) {
			}
			virtual void post_process_target() {
			}



			virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
				parent::read(proxy, oneliner, is_sample);
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

					("host", sh::string_fun_key<std::string>(boost::bind(&target_object::set_host, this, _1)),
					"TARGET HOST", "The target server to report results to.", true)

					("port", sh::string_fun_key<std::string>(boost::bind(&target_object::set_port, this, _1)),
					"TARGET PORT", "The target server port", true)

					;

				settings.register_all();
				settings.notify();

				BOOST_FOREACH(const target_object::options_type::value_type &kvp, options) {
					set_property_string(kvp.first, kvp.second);
				}

			}

			virtual void translate(const std::string &key, const std::string &value) {
				if (key == "host")
					set_host(value);
				else if (key == "port")
					set_port(value);
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

