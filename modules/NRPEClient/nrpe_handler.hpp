#pragma once

#include <utils.h>
#include <strEx.h>

#include <nsca/nsca_packet.hpp>

#include <nsca/client/nsca_client_protocol.hpp>
#include <socket/client.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <boost/make_shared.hpp>

namespace nrpe_handler {
	namespace sh = nscapi::settings_helper;

	struct nsca_target_object : public nscapi::targets::target_object {

		typedef nscapi::targets::target_object parent;

		nsca_target_object(std::string alias, std::string path) : parent(alias, path) {

			set_property_int("timeout", 30);
			set_property_string("certificate", "${certificate-path}/certificate.pem");
			set_property_string("certificate key", "");
			set_property_string("certificate format", "PEM");
			set_property_string("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			set_property_string("verify mode", "none");
			if (!has_option("insecure"))
				set_property_bool("insecure", false);
			set_property_bool("ssl", true);
			set_property_int("payload length", 1024);
		}


		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(this->path);
			if (is_sample)
				root_path.set_sample();

			add_ssl_keys(root_path);

			root_path.add_key()


				("insecure", sh::path_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "insecure", _1)),
				"Insecure legacy mode", "Use insecure legacy mode to connect to old NRPE server", false)

				("payload length",  sh::int_fun_key<int>(boost::bind(&parent::set_property_int, this, "payload length", _1)),
				"PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.")
			;
		}

	};

	struct options_reader_impl : public client::options_reader_interface {

		virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
			return boost::make_shared<nsca_target_object>(alias, path);
		}


		void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {

			add_ssl_options(desc, data);

			desc.add_options()
			
				("insecure", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&client::destination_container::set_bool_data, data, "insecure", _1)), 
				"Use insecure legacy mode")
			
				("payload-length,l", po::value<unsigned int>()->notifier(boost::bind(&client::destination_container::set_int_data, data, "payload length", _1)), 
				"Length of payload (has to be same as on the server)")

				("buffer-length", po::value<unsigned int>()->notifier(boost::bind(&client::destination_container::set_int_data, data, "payload length", _1)), 
				"Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.")

				("ssl", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&client::destination_container::set_bool_data, data, "ssl", _1)), 
				"Initial an ssl handshake with the server.")
				;
		}
	};

}