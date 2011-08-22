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


class NSCPClient : public nscapi::impl::simple_command, nscapi::impl::simple_plugin, public nscapi::impl::simple_command_line_exec {
private:
	struct nscp_connection_data {
		std::wstring host;
		std::wstring command;
		std::wstring command_line;
		std::vector<std::wstring> arguments;
		int port;
		int timeout;
		bool no_ssl;
		nscp_connection_data() 
			: host(_T("127.0.0.1")), 
			port(5668), 
			timeout(10), 
			no_ssl(false)
		{}
		std::wstring toString() {
			std::wstringstream ss;
			ss << _T("host: ") << host;
			ss << _T(", port: ") << port;
			ss << _T(", timeout: ") << timeout;
			ss << _T(", no_ssl: ") << no_ssl;
			ss << _T(", command: ") << command;
			int i=0;
			BOOST_FOREACH(std::wstring a, arguments) {
				ss << _T(", argument[") << i++ << _T("]: ") << a;
			}
			return ss.str();
		}
	};
	struct nscp_result_data {
		nscp_result_data() {}
		nscp_result_data(int result_, std::wstring text_) : result(result_), text(text_) {}
		std::wstring text;
		int result;
	};
	typedef std::map<std::wstring, nscp_connection_data> command_list;
	command_list commands;
	unsigned int buffer_length_;
	std::wstring cert_;

public:
	NSCPClient();
	virtual ~NSCPClient();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();


	std::wstring getModuleName() {
#ifdef USE_SSL
		return _T("NSCP client (w/ SSL)");
#else
		return _T("NSCP client");
#endif
	}
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::wstring getModuleDescription() {
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
	nscp_result_data  execute_nscp_command(nscp_connection_data con, std::string buffer);
	std::list<nscp::packet::nscp_chunk> send_nossl(std::wstring host, int port, int timeout, std::list<nscp::packet::nscp_chunk> &chunks);
	std::list<nscp::packet::nscp_chunk> send_ssl(std::wstring host, int port, int timeout, std::list<nscp::packet::nscp_chunk> &chunks);
	void add_options(po::options_description &desc, nscp_connection_data &command_data);

	NSCAPI::nagiosReturn query_nscp(std::list<std::wstring> &arguments, std::wstring &message, std::wstring perf);

private:
	void add_command(std::wstring key, std::wstring args);
	void add_server(std::wstring key, std::wstring args);

};

