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

#include <nsca/nsca_packet.hpp>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();
NSC_WRAPPERS_CHANNELS();

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
		}
	};

	nscapi::targets::handler<custom_reader> targets;
	client::command_manager commands;

public:
	struct connection_data {
		std::string password;
		std::string encryption;
		std::string host, port;
		std::string sender_hostname;
		int timeout;
		int buffer_length;
		int time_delta;

		connection_data(nscapi::protobuf::types::destination_container recipient, nscapi::protobuf::types::destination_container target, nscapi::protobuf::types::destination_container sender) {
			recipient.import(target);
			timeout = recipient.get_int_data("timeout", 30);
			buffer_length = recipient.get_int_data("payload length", 512);
			password = recipient.get_string_data("password");
			encryption = recipient.get_string_data("encryption");
			std::string tmp = recipient.get_string_data("time offset");
			if (!tmp.empty())
				time_delta = strEx::stol_as_time_sec(recipient.get_string_data("time offset"));
			else
				time_delta = 0;
			host = recipient.address.get_host();
			port = strEx::s::xtos(recipient.address.get_port(5667));
			sender_hostname = sender.address.host;
			if (sender.has_data("host"))
				sender_hostname = sender.get_string_data("host");
		}
		unsigned int get_encryption() {
			return nsca::nsca_encrypt::helpers::encryption_to_int(encryption);
		}

		std::wstring to_wstring() {
			std::wstringstream ss;
			ss << _T("host: ") << utf8::cvt<std::wstring>(host);
			ss << _T(", port: ") << utf8::cvt<std::wstring>(port);
			ss << _T(", timeout: ") << timeout;
			ss << _T(", buffer_length: ") << buffer_length;
			ss << _T(", time_delta: ") << time_delta;
			ss << _T(", password: ") << utf8::cvt<std::wstring>(password);
			ss << _T(", encryption: ") << utf8::cvt<std::wstring>(encryption);
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
#ifdef HAVE_LIBCRYPTOPP
		return _T("NSCAClient");
#else
		return _T("NSCAClient (without encryption support)");
#endif
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
		return std::wstring(_T("Passive check support (needs NSCA on nagios server).\nAvalible crypto are: ")) + getCryptos();
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

	static std::wstring getCryptos();
	void set_delay(std::wstring key) {
		time_delta_ = strEx::stol_as_time_sec(key, 1);
	}

};
