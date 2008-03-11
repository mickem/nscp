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

#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>
#include "WMIQuery.h"

class CheckWMI {
private:
	WMIQuery wmiQuery;

public:
	CheckWMI();
	virtual ~CheckWMI();
	// Module calls
	bool loadModule();
	bool unloadModule();

	std::wstring getModuleName() {
		return _T("CheckWMI");
	}
	std::wstring getModuleDescription() {
		return _T("CheckWMI can check various file and disk related things.\nThe current version has commands to check Size of hard drives and directories.");
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf);
	int CheckWMI::commandLineExec(const TCHAR* command,const unsigned int argLen,TCHAR** args);

	// Check commands
	NSCAPI::nagiosReturn CheckSimpleWMI(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf);
	NSCAPI::nagiosReturn CheckSimpleWMIValue(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf);



private:
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsDiscSize> PathConatiner;
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinPercentageBoundsDiskSize> DriveConatiner;
};