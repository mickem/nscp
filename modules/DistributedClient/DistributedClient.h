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

#include <client/command_line_parser.hpp>
#include <nscapi/targets.hpp>

#include <nscp/packet.hpp>

#include <zmq.h>
#include <zmq.hpp>
#include <zmsg.hpp>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();
NSC_WRAPPERS_CHANNELS();

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class DistributedClient : public nscapi::impl::simple_plugin {
private:

	std::wstring channel_;
	std::wstring target_path;
	const static std::wstring command_prefix;

	struct custom_reader {
		typedef nscapi::targets::target_object object_type;
		typedef nscapi::targets::target_object target_object;

		static void init_default(target_object &target) {
			target.set_property_int(_T("timeout"), 30);
			target.set_property_bool(_T("ssl"), true);
			target.set_property_string(_T("certificate"), _T("${certificate-path}/nrpe_dh_512.pem"));
			target.set_property_int(_T("payload length"), 1024);
		}

		static void add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
			settings.path(object.path).add_key()

				(_T("timeout"), sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, _T("timeout"), _1), 30),
				_T("TIMEOUT"), _T("Timeout when reading/writing packets to/from sockets."))

				;
		}

		static void post_process_target(target_object &target) {
			nscapi::core_wrapper* core = GET_CORE();
			if (core == NULL) {
				NSC_LOG_ERROR_STD(_T("Invalid core"));
				return;
			}
			if (target.has_option(_T("certificate"))) {
				std::wstring value = target.options[_T("certificate")];
				boost::filesystem::wpath p = value;
				if (!boost::filesystem::is_regular(p)) {
					p = core->getBasePath() / p;
					if (boost::filesystem::is_regular(p)) {
						value = p.string();
					} else {
						value = core->expand_path(value);
					}
					target.options[_T("certificate")] = value;
				}
				if (boost::filesystem::is_regular(p)) {
					NSC_DEBUG_MSG_STD(_T("Using certificate: ") + p.string());
				} else {
					NSC_LOG_ERROR_STD(_T("Certificate not found: ") + p.string());
				}
			}
		}

	};

	nscapi::targets::handler<custom_reader> targets;
	client::command_manager commands;

	struct connection_data {
		std::string address;
		int timeout;

		connection_data(nscapi::functions::destination_container arguments, nscapi::functions::destination_container target) {
			arguments.import(target);
			timeout = arguments.get_int_data("timeout", 30);
			address = arguments.address.to_string();
		}

		std::wstring to_wstring() const {
			std::wstringstream ss;
			ss << _T("address: ") << utf8::cvt<std::wstring>(address);
			ss << _T(", timeout: ") << timeout;
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		DistributedClient *instance;
		clp_handler_impl(DistributedClient *instance) : instance(instance) {}

		int query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, std::string &reply);
		int submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, std::string &reply);
		int exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, std::string &reply);

		virtual nscapi::functions::destination_container lookup_target(std::wstring &id) {
			nscapi::targets::optional_target_object opt = instance->targets.find_object(id);
			if (opt)
				return opt->to_destination_container();
			nscapi::functions::destination_container ret;
			return ret;
		}
	};


public:
	DistributedClient();
	virtual ~DistributedClient();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	/**
	* Return the module name.
	* @return The module name
	*/
	static std::wstring getModuleName() {
		return _T("DistributedClient");
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
		return _T("A client for connecting to a distributed Server.\n");
	}

	bool hasCommandHandler() { return true; };
	bool hasMessageHandler() { return true; };
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);

private:
	std::list<std::string> submit_nscp_command(connection_data con, std::string buffer);
	std::list<std::string> execute_nscp_command(connection_data con, std::string buffer);
	std::list<std::string> execute_nscp_query(connection_data con, std::string buffer);
	std::list<nscp::packet> send(connection_data &data, std::list<nscp::packet> &chunks);
	std::list<nscp::packet> send_nossl(std::string host, int timeout, const std::list<nscp::packet> &chunks);


	NSCAPI::nagiosReturn query_nscp(std::list<std::wstring> &arguments, std::wstring &message, std::wstring perf);
	bool submit_nscp(std::list<std::wstring> &arguments, std::wstring &result);

	static connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config, const ::Plugin::Common_Header& header);
	void add_command(std::wstring key, std::wstring args);
	void add_target(std::wstring key, std::wstring args);

};

