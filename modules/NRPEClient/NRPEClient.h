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
#include <map>
#include <nrpe/nrpe_packet.hpp>


class NRPEClient : public NSCModuleHelper::SimpleCommand {
private:
	typedef enum {
		inject, script, script_dir,
	} command_type;
	struct nrpe_connection_data {
		std::wstring host;
		std::wstring command;
		std::wstring arguments;
		std::wstring command_line;
		std::vector<std::wstring> argument_vector;
		int port;
		int timeout;
		unsigned int buffer_length;
		bool no_ssl;
		nrpe_connection_data(unsigned int buffer_length_ = 1024) 
			: host(_T("127.0.0.1")), 
			port(5666), 
			timeout(10), 
			no_ssl(false), 
			buffer_length(buffer_length_) 
		{}
		void parse_arguments() {
			for (std::vector<std::wstring>::const_iterator cit = argument_vector.begin(); cit != argument_vector.end(); ++cit) {
				if (!arguments.empty())
					arguments += _T("!");
				arguments += *cit;
			}
		}
		std::wstring get_cli(std::wstring arguments_) {
			if (command_line.empty()) {
				command_line = command;
				if (command_line.empty())
					command_line = _T("_NRPE_CHECK");
				if (!arguments_.empty())
					command_line += _T("!") + arguments_;
				else if (!arguments.empty())
					command_line += _T("!") + arguments;
			}
			return command_line;
		}
		std::wstring toString() {
			std::wstringstream ss;
			ss << _T("host: ") << host;
			ss << _T(", port: ") << port;
			ss << _T(", timeout: ") << timeout;
			ss << _T(", no_ssl: ") << no_ssl;
			ss << _T(", buffer_length: ") << buffer_length;
			ss << _T(", command: ") << command;
			ss << _T(", argument: ") << arguments;
			return ss.str();
		}
	};
	struct nrpe_result_data {
		nrpe_result_data() {}
		nrpe_result_data(int result_, std::wstring text_) : result(result_), text(text_) {}
		std::wstring text;
		int result;
	};
	typedef std::map<strEx::blindstr, nrpe_connection_data> command_list;
	command_list commands;
	unsigned int buffer_length_;
	std::wstring cert_;

public:
	NRPEClient();
	virtual ~NRPEClient();
	// Module calls
	bool loadModule(NSCAPI::moduleLoadMode mode);
	bool unloadModule();


	std::wstring getModuleName() {
#ifdef USE_SSL
		return _T("NRPE client (w/ SSL)");
#else
		return _T("NRPE client");
#endif
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("A simple client for checking remote NRPE servers (think proxy).\n")
#ifndef USE_SSL
		_T("SSL support is missing (so you cant use encryption)!")
#endif
	;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);
	int commandLineExec(const unsigned int argLen,TCHAR** args);
	std::wstring getConfigurationMeta();

private:
	nrpe_result_data  execute_nrpe_command(nrpe_connection_data con, std::wstring arguments);
	nrpe::packet send_nossl(std::wstring host, int port, int timeout, nrpe::packet packet);
	nrpe::packet send_ssl(std::wstring host, int port, int timeout, nrpe::packet packet);
	void add_options(po::options_description &desc, nrpe_connection_data &command_data);

private:
	void addCommand(strEx::blindstr key, std::wstring args);

};

