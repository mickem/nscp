#pragma once

#include <utils.h>
#include <strEx.h>

#include <socket/client.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <boost/make_shared.hpp>

namespace smtp_handler {
	namespace sh = nscapi::settings_helper;

	struct smtp_target_object : public nscapi::targets::target_object {

		typedef nscapi::targets::target_object parent;

		smtp_target_object(std::string alias, std::string path) : parent(alias, path) {
			set_property_int("timeout", 30);
			set_property_string("sender", "nscp@localhost");
			set_property_string("recipient", "nscp@localhost");
			set_property_string("template", "Hello, this is %source% reporting %message%!");
		}


		virtual void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			parent::read(proxy, oneliner, is_sample);

			nscapi::settings_helper::settings_registry settings(proxy);

			nscapi::settings_helper::path_extension root_path = settings.path(this->path);
			if (is_sample)
				root_path.set_sample();

			root_path.add_key()

				("sender", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "sender", _1), "nscp@localhost"),
				"SENDER", "Sender of email message")

				("recipient", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "recipient", _1), "nscp@localhost"),
				"RECIPIENT", "Recipient of email message")

				("template", sh::string_fun_key<std::string>(boost::bind(&parent::set_property_string, this, "template", _1), "Hello, this is %source% reporting %message%!"),
				"TEMPLATE", "Template for message data")		

				;
		}

	};

	struct options_reader_impl : public client::options_reader_interface {

		virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
			return boost::make_shared<smtp_target_object>(alias, path);
		}


		void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {

			desc.add_options()

				("sender", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, source, "sender", _1)), 
				"Length of payload (has to be same as on the server)")

				("recipient", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "recipient", _1)), 
				"Length of payload (has to be same as on the server)")

				("template", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, data, "template", _1)), 
				"Do not initial an ssl handshake with the server, talk in plain text.")

				("source-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "host", _1)), 
				"Source/sender host name (default is auto which means use the name of the actual host)")

				("sender-host", po::value<std::string>()->notifier(boost::bind(&client::destination_container::set_string_data, &source, "host", _1)), 
				"Source/sender host name (default is auto which means use the name of the actual host)")

				;
		}
	};

}