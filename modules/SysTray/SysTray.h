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

#include "TrayIcon.h"
#include <list>

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();


#define MODULE_NAME "SystemTray"
class SysTray {
private:
	IconWidget icon;

public:
	struct log_entry {
		log_entry(int msgType_, std::wstring file_, int line_, std::wstring message_) : type(msgType_), file(file_), line(line_), message(message_) {

		}
		int type;
		std::wstring file;
		int line;
		std::wstring message;
	};
	typedef std::list<log_entry> log_type;
private:
	log_type log;
	MutexHandler logLock;
	HWND hLogWnd;

public:
	SysTray();
	virtual ~SysTray();
	// Module calls
	bool loadModule(NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void setLogWindow(HWND hWnd);

	std::wstring getModuleName() {
		return _T(MODULE_NAME);
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}

	std::wstring getModuleDescription() {
		return _T("A simple module that only displays a system tray icon when NSClient++ is running.");
	}
	log_type getLog();


	bool hasCommandHandler();
	bool hasMessageHandler();
	int commandLineExec(const TCHAR* command, const unsigned int argLen, TCHAR** args);
	void handleMessage(int msgType, TCHAR* file, int line, TCHAR* message);

};