/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <boost/tuple/tuple.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>
#include <socket/client.hpp>


#include <nsca/nsca_packet.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class NSCAClient : public nscapi::impl::simple_plugin {
private:

	std::string channel_;
	std::string target_path;
	std::string hostname_;
	bool cacheNscaHost_;
	long time_delta_;
	std::string encoding_;

	struct custom_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_int("timeout", 30);
			target.set_property_string("encryption", "ase");
			target.set_property_int("payload length", 512);
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool is_sample) {
			nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
			if (is_sample)
				root_path.set_sample();
			root_path.add_key()

				("timeout", sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, "timeout", _1), 30),
				"TIMEOUT", "Timeout when reading/writing packets to/from sockets.")

				("dh", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "dh", _1), "${certificate-path}/nrpe_dh_512.pem"),
				"DH KEY", "", true)

				("certificate", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate", _1)),
				"SSL CERTIFICATE", "", false)

				("certificate key", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate key", _1)),
				"SSL CERTIFICATE", "", true)

				("certificate format", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate format", _1), "PEM"),
				"CERTIFICATE FORMAT", "", true)

				("ca", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "ca", _1)),
				"CA", "", true)

				("allowed ciphers", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "allowed ciphers", _1), "ADH"),
				"ALLOWED CIPHERS", "A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", false)

				("verify mode", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "verify mode", _1), "none"),
				"VERIFY MODE", "", false)

				("use ssl", sh::bool_fun_key<bool>(boost::bind(&object_type::set_property_bool, &object, "ssl", _1), false),
				"ENABLE SSL ENCRYPTION", "This option controls if SSL should be enabled.")

				("payload length",  sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, "payload length", _1), 512),
				"PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.", true)

				("encryption", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "encryption", _1), "aes"),
				"ENCRYPTION", std::string("Name of encryption algorithm to use.\nHas to be the same as your server i using or it wont work at all."
				"This is also independent of SSL and generally used instead of SSL.\nAvailable encryption algorithms are:\n") + nscp::encryption::helpers::get_crypto_string("\n"))

				("password", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "password", _1), ""),
				"PASSWORD", "The password to use. Again has to be the same as the server or it wont work at all.")

				("encoding", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "encoding", _1), ""),
				"ENCODING", "", true)

				("time offset", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "delay", _1), "0"),
				"TIME OFFSET", "Time offset.", true)
				;
		}

		static void post_process_target(target_object &target) {
			std::list<std::string> err;
			nscapi::targets::helpers::verify_file(target, "certificate", err);
			nscapi::targets::helpers::verify_file(target, "dh", err);
			nscapi::targets::helpers::verify_file(target, "certificate key", err);
			nscapi::targets::helpers::verify_file(target, "ca", err);
			BOOST_FOREACH(const std::string &e, err) {
				NSC_LOG_ERROR(e);
			}
		}
	};

	nscapi::targets::handler<custom_reader> targets;
	client::command_manager commands;

public:
	struct connection_data : public socket_helpers::connection_info {
		std::string password;
		std::string encryption;
		std::string sender_hostname;
		int buffer_length;
		int time_delta;
		std::string encoding;

		connection_data(nscapi::protobuf::types::destination_container arguments, nscapi::protobuf::types::destination_container target, nscapi::protobuf::types::destination_container sender) {
			arguments.import(target);
			address = arguments.address.host;
			port_ = arguments.address.get_port_string("5667");
			ssl.enabled = arguments.get_bool_data("ssl");
			ssl.certificate = arguments.get_string_data("certificate");
			ssl.certificate_key = arguments.get_string_data("certificate key");
			ssl.certificate_key_format = arguments.get_string_data("certificate format");
			ssl.ca_path = arguments.get_string_data("ca");
			ssl.allowed_ciphers = arguments.get_string_data("allowed ciphers");
			ssl.dh_key = arguments.get_string_data("dh");
			ssl.verify_mode = arguments.get_string_data("verify mode");
			timeout = arguments.get_int_data("timeout", 30);
			buffer_length = arguments.get_int_data("payload length", 512);
			password = arguments.get_string_data("password");
			encryption = arguments.get_string_data("encryption");
			encoding = arguments.get_string_data("encoding");
			std::string tmp = arguments.get_string_data("time offset");
			if (!tmp.empty())
				time_delta = strEx::stol_as_time_sec(arguments.get_string_data("time offset"));
			else
				time_delta = 0;
			sender_hostname = sender.address.host;
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}
		unsigned int get_encryption() {
			return nscp::encryption::helpers::encryption_to_int(encryption);
		}

		std::wstring to_wstring() const {
			return utf8::cvt<std::wstring>(to_string());
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", buffer_length: " << buffer_length;
			ss << ", time_delta: " << time_delta;
			ss << ", password: " << password;
			ss << ", encryption: " << encryption << "(" << nscp::encryption::helpers::encryption_to_int(encryption) << ")";
			ss << ", hostname: " << sender_hostname;
			ss << ", encoding: " << encoding;
			ss << ", ssl: " << ssl.to_string();
			return ss.str();
		}
	};

	struct target_handler : public client::target_lookup_interface {
		target_handler(const nscapi::targets::handler<custom_reader> &targets) : targets_(targets) {}
		nscapi::protobuf::types::destination_container lookup_target(std::string &id) const;
		bool apply(nscapi::protobuf::types::destination_container &dst, const std::string key);
		bool has_object(std::string alias) const;
		const nscapi::targets::handler<custom_reader> &targets_;
	};

	struct clp_handler_impl : public client::clp_handler {

		int query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message);
		int submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message);
		int exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message);

		boost::tuple<int,std::string> send(connection_data data, const std::list<nsca::packet> packets);
	};


public:
	NSCAClient();
	virtual ~NSCAClient();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	bool commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);

private:
	void add_options(po::options_description &desc, connection_data &command_data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::string key, std::string args);
	void add_target(std::string key, std::string args);

	void set_delay(std::string key) {
		time_delta_ = strEx::stol_as_time_sec(key, 1);
	}

};
