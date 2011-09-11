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

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();


#include <map>
#include <nscp/packet.hpp>
#include <client/command_line_parser.hpp>
#include <settings/client/targets.hpp>
#include <nscapi/targets.hpp>

class NSCPClient : public nscapi::impl::simple_command_handler, public nscapi::impl::simple_plugin, public nscapi::impl::simple_command_line_exec {
private:
	std::wstring cert_;
	nscapi::target_handler targets;
	std::wstring target_path;
	struct nscp_connection_data : public client::nscp_cli_data {
		bool no_ssl;
		std::wstring cert;
		nscp_connection_data() : no_ssl(false) {}
		std::wstring to_wstring() {
			std::wstringstream ss;
			ss << _T(", cert: ") << cert;
			ss << _T(", no_ssl: ") << no_ssl;
			return ss.str();
		}
	};

	struct clp_handler_impl : public client::clp_handler {

		NSCPClient *instance;
		clp_handler_impl(NSCPClient *instance) : instance(instance) {}
		nscp_connection_data local_data;

		int query(client::configuration::data_type data, std::string request, std::string &reply);
		std::list<std::string> submit(client::configuration::data_type data, std::string request);
		int exec(client::configuration::data_type data, std::string request, std::string &reply);
	};

public:
	NSCPClient();
	virtual ~NSCPClient();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();


	static std::wstring getModuleName() {
#ifdef USE_SSL
		return _T("NSCP client (w/ SSL)");
#else
		return _T("NSCP client");
#endif
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("A simple client for checking remote NSCP servers (think proxy).\n")
#ifndef USE_SSL
		_T("SSL support is missing (so you cant use encryption)!")
#endif
	;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	int commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);
	std::wstring getConfigurationMeta();

private:
	std::list<std::string> submit_nscp_command(nscp_connection_data con, std::string buffer);
	std::list<std::string> execute_nscp_command(nscp_connection_data con, std::string buffer);
	std::list<std::string> execute_nscp_query(nscp_connection_data con, std::string buffer);
	std::list<nscp::packet> send(client::configuration::data_type generic_data, nscp_connection_data &data, std::list<nscp::packet> &chunks);
	std::list<nscp::packet> send_nossl(std::wstring host, int port, int timeout, const std::list<nscp::packet> &chunks);
	std::list<nscp::packet> send_ssl(std::wstring host, int port, std::wstring cert, int timeout, const std::list<nscp::packet> &chunks);
	void add_local_options(po::options_description &desc, nscp_connection_data &command_data);

	NSCAPI::nagiosReturn query_nscp(std::list<std::wstring> &arguments, std::wstring &message, std::wstring perf);
	bool submit_nscp(std::list<std::wstring> &arguments, std::wstring &result);

private:
	client::command_manager commands;
	void add_command(std::wstring key, std::wstring args);
	void add_target(std::wstring key, std::wstring args);
	std::wstring setup(client::configuration config, const std::wstring &command);

};

