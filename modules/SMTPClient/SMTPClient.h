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


#include <client/command_line_parser.hpp>
#include <nscapi/targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();
NSC_WRAPPERS_CHANNELS();

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class SMTPClient : public nscapi::impl::simple_plugin {
private:

	std::wstring channel_;
	std::wstring target_path;
	const static std::wstring command_prefix;

	struct custom_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_int(_T("timeout"), 30);
			target.set_property_string(_T("sender"), _T("nscp@localhost"));
			target.set_property_string(_T("recipient"), _T("nscp@localhost"));
			target.set_property_string(_T("template"), _T("Hello, this is %source% reporting %message%!"));
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
			settings.path(object.path).add_key()

				(_T("timeout"), sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, _T("timeout"), _1), 30),
				_T("TIMEOUT"), _T("Timeout when reading/writing packets to/from sockets."))

				(_T("sender"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("sender"), _1), _T("nscp@localhost")),
				_T("SENDER"), _T("Sender of email message"))

				(_T("recipient"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("recipient"), _1), _T("nscp@localhost")),
				_T("RECIPIENT"), _T("Recipient of email message"))

				(_T("template"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_property_string, &object, _T("template"), _1), _T("Hello, this is %source% reporting %message%!")),
				_T("TEMPLATE"), _T("Template for message data"))
			;
		}
		static void post_process_target(target_object &target) {
		}
	};

	nscapi::targets::handler<custom_reader> targets;
	client::command_manager commands;

	struct connection_data {
		std::string recipient_str;
		std::string sender;
		std::string template_string;
		std::string host;
		std::string port;
		int timeout;

		connection_data(nscapi::protobuf::types::destination_container arguments, nscapi::protobuf::types::destination_container target) {
			arguments.import(target);
			recipient_str = arguments.get_string_data("recipient");
			timeout = arguments.get_int_data("timeout", 30);
			sender = arguments.get_string_data("sender");
			template_string = arguments.get_string_data("template");

			host = arguments.address.host;
			port = arguments.address.get_port(25);
		}

		std::wstring to_wstring() const {
			std::wstringstream ss;
			ss << _T("host: ") << utf8::cvt<std::wstring>(host);
			ss << _T(", port: ") << utf8::cvt<std::wstring>(port);
			ss << _T(", timeout: ") << timeout;
			ss << _T(", recipient: ") << utf8::cvt<std::wstring>(recipient_str);
			ss << _T(", sender: ") << utf8::cvt<std::wstring>(sender);
			ss << _T(", template: ") << utf8::cvt<std::wstring>(template_string);
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		SMTPClient *instance;
		clp_handler_impl(SMTPClient *instance) : instance(instance) {}

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
	SMTPClient();
	virtual ~SMTPClient();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	/**
	* Return the module name.
	* @return The module name
	*/
	static std::wstring getModuleName() {
		return _T("SMTPClient");
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
		return _T("Passive check support via SMTP");
	}

	bool hasCommandHandler() { return true; };
	bool hasMessageHandler() { return true; };
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);

private:
	static connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::wstring key, std::wstring args);
	void add_target(std::wstring key, std::wstring args);

};

