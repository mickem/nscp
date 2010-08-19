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

#include <pdh.hpp>
#include "PDHCollector.h"
#include <CheckMemory.h>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();

class CheckSystem : public nscapi::impl::SimpleCommand {
private:
	CheckMemory memoryChecker;
	PDHCollectorThread pdhThread;

public:
	typedef enum { started, stopped } states;
	typedef struct rB {
		NSCAPI::nagiosReturn code_;
		std::wstring msg_;
		std::wstring perf_;
		rB(NSCAPI::nagiosReturn code, std::wstring msg) : code_(code), msg_(msg) {}
		rB() : code_(NSCAPI::returnUNKNOWN) {}
	} returnBundle;

public:
	CheckSystem();
	virtual ~CheckSystem();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	std::wstring getConfigurationMeta();

	/**
	* Return the module name.
	* @return The module name
	*/
	std::wstring getModuleName() {
		return _T("CheckSystem");
	}
	/**
	* Module version
	* @return module version
	*/
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
		return version;
	}
	std::wstring getModuleDescription() {
		return _T("Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::wci_string command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);
	int commandLineExec(const TCHAR* command,const unsigned int argLen,TCHAR** args);

	NSCAPI::nagiosReturn checkCPU(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn checkUpTime(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn checkServiceState(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn checkMem(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn checkProcState(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn checkCounter(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn listCounterInstances(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn checkSingleRegEntry(std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);


};
