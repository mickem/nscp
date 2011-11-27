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

#include <nscp/packet.hpp>

#include <zmq.h>
#include <zmq.hpp>
#include <zmsg.hpp>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();
NSC_WRAPPERS_CHANNELS();

namespace po = boost::program_options;


class DistributedClient : public nscapi::impl::simple_command_handler, public nscapi::impl::simple_plugin, public nscapi::impl::simple_command_line_exec {
private:

	std::wstring channel_;
	std::wstring target_path;

	nscapi::target_handler targets;
	client::command_manager commands;

	struct connection_data {
		std::string cert;
		std::string address;
		int timeout;
		int buffer_length;
		bool use_ssl;

		connection_data(nscapi::functions::destination_container recipient) {
			cert = recipient.get_string_data("certificate");
			timeout = recipient.get_int_data("timeout", 30);
			buffer_length = recipient.get_int_data("payload length", 1024);
			use_ssl = recipient.get_bool_data("ssl");
			if (recipient.has_data("no ssl"))
				use_ssl = !recipient.get_bool_data("no ssl");
			address = recipient.address;
		}

		std::wstring to_wstring() const {
			std::wstringstream ss;
			ss << _T("address: ") << utf8::cvt<std::wstring>(address);
			ss << _T(", timeout: ") << timeout;
			ss << _T(", buffer_length: ") << buffer_length;
			ss << _T(", use_ssl: ") << use_ssl;
			ss << _T(", certificate: ") << utf8::cvt<std::wstring>(cert);
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler, client::target_lookup_interface {

		DistributedClient *instance;
		clp_handler_impl(DistributedClient *instance) : instance(instance) {}

		int query(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);
		int submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);
		int exec(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);

		virtual nscapi::functions::destination_container lookup_target(std::wstring &id) {
			nscapi::functions::destination_container ret;
			nscapi::target_handler::optarget t = instance->targets.find_target(id);
			if (t) {
				if (!t->alias.empty())
					ret.id = utf8::cvt<std::string>(t->alias);
				if (!t->host.empty())
					ret.host = utf8::cvt<std::string>(t->host);
				if (t->has_option("address"))
					ret.address = utf8::cvt<std::string>(t->options[_T("address")]);
				else 
					ret.address = utf8::cvt<std::string>(t->host);
				BOOST_FOREACH(const nscapi::target_handler::target::options_type::value_type &kvp, t->options) {
					ret.data[utf8::cvt<std::string>(kvp.first)] = utf8::cvt<std::string>(kvp.second);
				}
			}
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
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	int commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);

private:
	std::list<std::string> submit_nscp_command(connection_data con, std::string buffer);
	std::list<std::string> execute_nscp_command(connection_data con, std::string buffer);
	std::list<std::string> execute_nscp_query(connection_data con, std::string buffer);
	std::list<nscp::packet> send(connection_data &data, std::list<nscp::packet> &chunks);
	std::list<nscp::packet> send_nossl(std::string host, int timeout, const std::list<nscp::packet> &chunks);


	NSCAPI::nagiosReturn query_nscp(std::list<std::wstring> &arguments, std::wstring &message, std::wstring perf);
	bool submit_nscp(std::list<std::wstring> &arguments, std::wstring &result);

	static connection_data parse_header(const ::Plugin::Common_Header &header);

private:
	void add_local_options(po::options_description &desc, client::configuration::data_type data);
	void setup(client::configuration &config);
	void add_command(std::wstring key, std::wstring args);
	void add_target(std::wstring key, std::wstring args);

};

