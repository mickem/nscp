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
#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>

class CheckDisk : public nscapi::impl::SimpleCommand {
private:
	bool show_errors_;

public:
	CheckDisk();
	virtual ~CheckDisk();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	std::wstring getModuleName() {
		return _T("CheckDisk");
	}
	std::wstring getModuleDescription() {
		return _T("CheckDisk can check various file and disk related things.\nThe current version has commands to check Size of hard drives and directories.");
	}
	nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	std::wstring get_filter(unsigned int drvType);
	NSCAPI::nagiosReturn handleCommand(const strEx::wci_string command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);

	// Check commands
	NSCAPI::nagiosReturn CheckFileSize(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn CheckDriveSize(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn CheckFile2(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn getFileAge(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);
	NSCAPI::nagiosReturn CheckSingleFile(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf);

private:
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDiscSize> PathContainer;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinPercentageBoundsDiskSize> DriveContainer;
};
