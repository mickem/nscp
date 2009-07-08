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
#include <error.hpp>
#include <execute_process.hpp>

class CheckExternalScripts {
private:
	struct command_data {
		command_data() {}
		command_data(std::wstring command_, std::wstring arguments_) : command(command_), arguments(arguments_) {}
		std::wstring command;
		std::wstring arguments;
	};
	typedef std::map<strEx::blindstr, command_data> command_list;
	command_list commands;
	command_list alias;
	unsigned int timeout;
	std::wstring scriptDirectory_;
	std::wstring root_;

public:
	CheckExternalScripts();
	virtual ~CheckExternalScripts();
	// Module calls
	bool loadModule();
	bool unloadModule();


	std::wstring getModuleName() {
		return _T("Check External Scripts");
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("A simple wrapper to run external scripts and batch files.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf);
	std::wstring getConfigurationMeta();

private:
	class NRPEException {
		std::wstring error_;
	public:
		NRPEException(std::wstring s) {
			error_ = s;
		}
		std::wstring getMessage() {
			return error_;
		}
	};


private:
	void addAllScriptsFrom(std::wstring path);
	void addCommand(strEx::blindstr key, std::wstring cmd, std::wstring args) {
		commands[key] = command_data(cmd, args);
	}
	void addAlias(strEx::blindstr key, std::wstring cmd, std::wstring args) {
		alias[key] = command_data(cmd, args);
	}
};

