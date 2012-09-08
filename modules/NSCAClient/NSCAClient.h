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

#include <client/command_line_parser.hpp>
#include <nscapi/targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>

#include <socket/client.hpp>

#include <nsca/nsca_packet.hpp>

NSC_WRAPPERS_MAIN()
NSC_WRAPPERS_CLI()
NSC_WRAPPERS_CHANNELS()

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class NSCAAgent : public nscapi::impl::simple_plugin {
private:

	std::wstring channel_;
	std::wstring target_path;
	const static std::wstring command_prefix;
	std::string hostname_;
	bool cacheNscaHost_;
	long time_delta_;

	struct custom_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_int(_T("timeout"), 30);
			target.set_property_string(_T("encryption"), _T("ase"));
			target.set_property_int(_T("payload length"), 512);
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
			settings.path(object.path).add_key()

				(_T("timeout"), sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, _T("timeout"), _1), 30),
				_T("TIMEOUT"), _T("Timeout when reading/writing packets to/from sockets."))

				(_T("dh"), sh::path_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("dh"), _1), _T("${certificate-path}/nrpe_dh_512.pem")),
				_T("DH KEY"), _T(""), true)

				(_T("certificate"), sh::path_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("certificate"), _1)),
				_T("SSL CERTIFICATE"), _T(""), false)

				(_T("certificate key"), sh::path_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("certificate key"), _1)),
				_T("SSL CERTIFICATE"), _T(""), true)

				(_T("certificate format"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("certificate format"), _1), _T("PEM")),
				_T("CERTIFICATE FORMAT"), _T(""), true)

				(_T("ca"), sh::path_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("ca"), _1)),
				_T("CA"), _T(""), true)

				(_T("allowed ciphers"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("allowed ciphers"), _1), _T("ADH")),
				_T("ALLOWED CIPHERS"), _T("A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"), false)

				(_T("verify mode"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("verify mode"), _1), _T("none")),
				_T("VERIFY MODE"), _T(""), false)

				(_T("use ssl"), sh::bool_fun_key<bool>(boost::bind(&object_type::set_property_bool, &object, _T("ssl"), _1), true),
				_T("ENABLE SSL ENCRYPTION"), _T("This option controls if SSL should be enabled."))

				(_T("payload length"),  sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, _T("payload length"), _1), 512),
				_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work."), true)

				(_T("encryption"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("encryption"), _1), _T("aes")),
				_T("ENCRYPTION METHOD"), _T("Number corresponding to the various encryption algorithms (see the wiki). Has to be the same as the server or it wont work at all."))

				(_T("password"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("password"), _1), _T("")),
				_T("PASSWORD"), _T("The password to use. Again has to be the same as the server or it wont work at all."))

				(_T("time offset"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("delay"), _1), _T("0")),
				_T("TIME OFFSET"), _T("Time offset."), true)
				;
		}

		static void post_process_target(target_object &target) {
			std::list<std::wstring> err;
			nscapi::targets::helpers::verify_file(target, _T("certificate"), err);
			nscapi::targets::helpers::verify_file(target, _T("dh"), err);
			nscapi::targets::helpers::verify_file(target, _T("certificate key"), err);
			nscapi::targets::helpers::verify_file(target, _T("ca"), err);
			BOOST_FOREACH(const std::wstring &e, err) {
				NSC_LOG_ERROR_STD(e);
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
			ss << ", ssl: " << ssl.to_string();
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		NSCAAgent *instance;
		clp_handler_impl(NSCAAgent *instance) : instance(instance) {}

		int query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, std::string &reply);
		int submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, std::string &reply);
		int exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, std::string &reply);

		virtual nscapi::protobuf::types::destination_container lookup_target(std::wstring &id) {
			nscapi::targets::optional_target_object opt = instance->targets.find_object(id);
			if (opt)
				return opt->to_destination_container();
			nscapi::protobuf::types::destination_container ret;
			return ret;
		}
	};


public:
	NSCAAgent();
	virtual ~NSCAAgent();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	/**
	* Return the module name.
	* @return The module name
	*/
	static std::wstring getModuleName() {
		return _T("NSCAClient");
	}
	/**
	* Module version
	* @return module version
	*/
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 4, 0 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("Passive check support over NSCA.");
	}

	bool hasCommandHandler() { return true; };
	bool hasMessageHandler() { return true; };
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);

private:
	boost::tuple<int,std::wstring> send(connection_data data, const std::list<nsca::packet> packets);
	void add_options(po::options_description &desc, connection_data &command_data);
	static connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::wstring key, std::wstring args);
	void add_target(std::wstring key, std::wstring args);

	void set_delay(std::wstring key) {
		time_delta_ = strEx::stol_as_time_sec(key, 1);
	}

};
