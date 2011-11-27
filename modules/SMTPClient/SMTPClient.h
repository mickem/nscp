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
	std::wstring channel_;
	nscapi::target_handler targets;
	std::wstring target_path;

	struct connection_data : public client::nscp_cli_data {};
	struct clp_handler_impl : public client::clp_handler {

		SMTPClient *instance;
		clp_handler_impl(SMTPClient *instance) : instance(instance) {}
		connection_data local_data;

		int query(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);
		int submit(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);
		int exec(client::configuration::data_type data, ::Plugin::Common_Header* header, const std::string &request, std::string &reply);
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

	void setup(client::configuration &config);
	void add_local_options(boost::program_options::options_description &desc, connection_data &command_data);
	void add_target(std::wstring key, std::wstring args);

};
