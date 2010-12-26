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
#include "PDHCollector.h"

class DebugLogMetrics {
private:
// 	struct command_data {
// 		command_data() {}
// 		command_data(std::wstring command_, std::wstring arguments_) : command(command_), arguments(arguments_) {}
// 		std::wstring command;
// 		std::wstring arguments;
// 	};
// 	typedef std::map<strEx::blindstr, command_data> command_list;
// 	command_list commands;
// 	command_list alias;
// 	unsigned int timeout;
// 	std::wstring scriptDirectory_;
// 	std::wstring root_;
	PDHCollectorThread pdhThread;

public:
	DebugLogMetrics();
	virtual ~DebugLogMetrics();
	// Module calls
	bool loadModule();
	bool unloadModule();


	std::wstring getModuleName() {
		return _T("Metrics Logger");
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("A module to log metrics to a file to find reasource and memory leaks.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf);
	std::wstring getConfigurationMeta();

};

