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
#include <boost/program_options.hpp>

#include <nscapi/targets.hpp>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CHANNELS();

class SMTPClient : public nscapi::impl::simple_plugin {
private:

	std::string hostname_;
	unsigned int payload_length_;
	unsigned int timeout_;
	std::wstring channel_;
	int time_delta_;
	nscapi::target_handler targets;
	std::wstring target_path;


	struct sender_information {
		sender_information(nscapi::functions::destination_container &src) : timeout(10) {
			net::url u = src.get_url(25);
			host = u.host;
			port = u.port;
			//password = src.data["password"];
			//encryption = src.data["encryption"];
		}
		std::string password;
		std::string encryption;
		std::string host;
		unsigned int timeout;
		int port;
	};
	struct connection_data : public client::nscp_cli_data {};
	struct clp_handler_impl : public client::clp_handler {

		SMTPClient *instance;
		clp_handler_impl(SMTPClient *instance, unsigned int payload_length, int time_delta) : instance(instance) {}
		connection_data local_data;

		int query(client::configuration::data_type data, std::string request, std::string &reply);
		int submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &response);
		int exec(client::configuration::data_type data, std::string request, std::string &reply);
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
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("Passive check support via SMTP");
	}
	bool hasNotificationHandler() { return true; }

	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	int commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);

	NSCAPI::nagiosReturn send(sender_information &data, const std::list<nsca::packet> packets);


	std::wstring setup(client::configuration &config, const std::wstring &command);
	void add_local_options(boost::program_options::options_description &desc, connection_data &command_data);
	void add_target(std::wstring key, std::wstring args);

};
