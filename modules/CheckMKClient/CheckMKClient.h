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
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <client/command_line_parser.hpp>
#include <nscapi/targets.hpp>
#include <nscapi/nscapi_protobuf_types.hpp>

#include <socket/client.hpp>

#include <check_mk/client/client_protocol.hpp>
#include <check_mk/lua/lua_check_mk.hpp>

NSC_WRAPPERS_MAIN()
NSC_WRAPPERS_CLI()
NSC_WRAPPERS_CHANNELS()

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class CheckMKClient : public nscapi::impl::simple_plugin {
private:
	boost::scoped_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
	boost::shared_ptr<lua::lua_runtime> lua_runtime_;
	boost::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime_;
	boost::filesystem::path root_;
	std::wstring channel_;
	std::wstring target_path;
	const static std::wstring command_prefix;

	struct custom_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_int(_T("timeout"), 30);
			target.set_property_bool(_T("ssl"), false);
			target.set_property_int(_T("payload length"), 1024);
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

		connection_data(nscapi::protobuf::types::destination_container arguments, nscapi::protobuf::types::destination_container target) {
			arguments.import(target);
			address = arguments.address.host;
			port_ = arguments.address.get_port_string("5666");
			ssl.enabled = arguments.get_bool_data("ssl");
			ssl.certificate = arguments.get_string_data("certificate");
			ssl.certificate_key = arguments.get_string_data("certificate key");
			ssl.certificate_key_format = arguments.get_string_data("certificate format");
			ssl.ca_path = arguments.get_string_data("ca");
			ssl.allowed_ciphers = arguments.get_string_data("allowed ciphers");
			ssl.dh_key = arguments.get_string_data("dh");
			ssl.verify_mode = arguments.get_string_data("verify mode");
			timeout = arguments.get_int_data("timeout", 30);

			if (arguments.has_data("no ssl"))
				ssl.enabled = !arguments.get_bool_data("no ssl");
			if (arguments.has_data("use ssl"))
				ssl.enabled = arguments.get_bool_data("use ssl");


		}

		std::wstring to_wstring() const {
			return utf8::cvt<std::wstring>(to_string());
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "host: " << get_endpoint_string();
			ss << ", ssl: " << ssl.to_string();
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		CheckMKClient *instance;
		clp_handler_impl(CheckMKClient *instance) : instance(instance) {}

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
	CheckMKClient();
	virtual ~CheckMKClient();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	/**
	* Return the module name.
	* @return The module name
	*/
	static std::wstring getModuleName() {
		return _T("check_mk client");
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
		return _T("A simple check_mk client for checking remote check_mk servers.");
	}

	bool hasCommandHandler() { return true; };
	bool hasMessageHandler() { return true; };
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);

private:
	void send(connection_data con);
	bool add_script(std::wstring alias, std::wstring file);


	NSCAPI::nagiosReturn query_nscp(std::list<std::wstring> &arguments, std::wstring &message, std::wstring perf);
	bool submit_nscp(std::list<std::wstring> &arguments, std::wstring &result);

	static connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::wstring key, std::wstring args);
	void add_target(std::wstring key, std::wstring args);
	NSCAPI::nagiosReturn parse_data(lua::script_information *information, lua::lua_traits::function_type c, const check_mk::packet &packet);

};

