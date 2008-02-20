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
#include <Socket.h>
#include <SSLSocket.h>
#include <map>
#include <nrpe/NRPEPacket.hpp>
#include <boost/program_options.hpp>


class NRPEClient {
private:
	typedef enum {
		inject, script, script_dir,
	} command_type;
	struct nrpe_connection_data {
		std::wstring host;
		std::wstring command;
		std::wstring arguments;
		std::wstring command_line;
		int port;
		int timeout;
		unsigned int buffer_length;
		bool ssl;
		nrpe_connection_data(unsigned int buffer_length_ = 1024) 
			: host(_T("localhost")), 
			port(5666), 
			timeout(10), 
			ssl(true), 
			buffer_length(buffer_length_) 
		{}
		std::wstring get_cli() {
			if (command_line.empty()) {
				command_line = command;
				if (command_line.empty())
					command_line = _T("_NRPE_CHECK");
				if (!arguments.empty())
					command_line += _T("!") + arguments;
			}
			return command_line;
		}
		std::wstring toString() {
			std::wstringstream ss;
			ss << _T("host: ") << host;
			ss << _T(", port: ") << port;
			ss << _T(", timeout: ") << timeout;
			ss << _T(", ssl: ") << ssl;
			ss << _T(", buffer_length: ") << buffer_length;
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

public:
	NRPEClient();
	virtual ~NRPEClient();
	// Module calls
	bool loadModule();
	bool unloadModule();


	std::wstring getModuleName() {
		return _T("NRPE client");
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("A simple client for NRPE.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf);
	int commandLineExec(const TCHAR* command,const unsigned int argLen,TCHAR** args);
	std::wstring getConfigurationMeta();

private:
	nrpe_result_data  execute_nrpe_command(nrpe_connection_data con);
	NRPEPacket send_nossl(std::wstring host, int port, int timeout, NRPEPacket packet);
	NRPEPacket send_ssl(std::wstring host, int port, int timeout, NRPEPacket packet);

	boost::program_options::options_description NRPEClient::get_optionDesc();
	boost::program_options::positional_options_description NRPEClient::get_optionsPositional();
	nrpe_connection_data NRPEClient::get_ConectionData(boost::program_options::variables_map &vm);


private:
	void addCommand(strEx::blindstr key, std::wstring args);

};

