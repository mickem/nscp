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
#include "stdafx.h"
#include "RemoteConfiguration.h"
#include <strEx.h>
#include <time.h>


RemoteConfiguration gRemoteConfiguration;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

RemoteConfiguration::RemoteConfiguration() {
}
RemoteConfiguration::~RemoteConfiguration() {
}


bool RemoteConfiguration::loadModule() {
	return true;
}
bool RemoteConfiguration::unloadModule() {
	return true;
}

bool RemoteConfiguration::hasCommandHandler() {
	return true;
}
bool RemoteConfiguration::hasMessageHandler() {
	return false;
}

// set writeConf type
NSCAPI::nagiosReturn RemoteConfiguration::writeConf(const unsigned int argLen, TCHAR **char_args, std::wstring &message) {
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	if (args.size() > 0) {
		if (args.front() == _T("reg")) {
			if (NSCModuleHelper::WriteSettings(NSCAPI::settings_registry) == NSCAPI::isSuccess) {
				message = _T("Settings written successfully.");
				return NSCAPI::returnOK;
			}
			message = _T("ERROR could not write settings.");
			return NSCAPI::returnCRIT;
		}
	}
	if (NSCModuleHelper::WriteSettings(NSCAPI::settings_inifile) == NSCAPI::isSuccess) {
		message = _T("Settings written successfully.");
		return NSCAPI::returnOK;
	}
	message = _T("ERROR could not write settings.");
	return NSCAPI::returnCRIT;
}

NSCAPI::nagiosReturn RemoteConfiguration::readConf(const unsigned int argLen, TCHAR **char_args, std::wstring &message) {
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	if (args.size() > 0) {
		if (args.front() == _T("reg")) {
			if (NSCModuleHelper::ReadSettings(NSCAPI::settings_registry) == NSCAPI::isSuccess) {
				message = _T("Settings written successfully.");
				return NSCAPI::returnOK;
			}
			message = _T("ERROR could not write settings.");
			return NSCAPI::returnCRIT;
		}
	}
	if (NSCModuleHelper::ReadSettings(NSCAPI::settings_inifile) == NSCAPI::isSuccess) {
		message = _T("Settings written successfully.");
		return NSCAPI::returnOK;
	}
	message = _T("ERROR could not write settings.");
	return NSCAPI::returnCRIT;
}
// set setVariable int <section> <variable> <value>
NSCAPI::nagiosReturn RemoteConfiguration::setVariable(const unsigned int argLen, TCHAR **char_args, std::wstring &message) {
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.size() < 3) {
		message = _T("Invalid syntax.");
		return NSCAPI::returnUNKNOWN;
	}
	std::wstring type = args.front(); args.pop_front();
	std::wstring section = args.front(); args.pop_front();
	std::wstring key = args.front(); args.pop_front();
	std::wstring value;
	if (args.size() >= 1) {
		value = args.front();
	}
	if (type == _T("int")) {
		NSCModuleHelper::SetSettingsInt(section, key, strEx::stoi(value));
		message = _T("Settings ") + key + _T(" saved successfully.");
		return NSCAPI::returnOK;
	} else if (type == _T("string")) {
		NSCModuleHelper::SetSettingsString(section, key, value);
		message = _T("Settings ") + key + _T(" saved successfully.");
		return NSCAPI::returnOK;
	} else {
		NSCModuleHelper::SetSettingsString(type, section, key);
		message = _T("Settings ") + section + _T(" saved successfully.");
		return NSCAPI::returnOK;
	}
}
NSCAPI::nagiosReturn RemoteConfiguration::getVariable(const unsigned int argLen, TCHAR **char_args, std::wstring &message) {
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.size() < 2) {
		message = _T("Invalid syntax.");
		return NSCAPI::returnUNKNOWN;
	}
	std::wstring section = args.front(); args.pop_front();
	std::wstring key = args.front(); args.pop_front();
	std::wstring value;
	value = NSCModuleHelper::getSettingsString(section, key, _T(""));
	message = section+_T("/")+key+_T("=")+value;
	return NSCAPI::returnOK;
}
int RemoteConfiguration::commandLineExec(const TCHAR* command,const unsigned int argLen,TCHAR** args) {
	std::wstring str;
	if (_wcsicmp(command, _T("setVariable")) == 0) {
		setVariable(argLen, args, str);
	} else if (_wcsicmp(command, _T("writeConf")) == 0) {
		writeConf(argLen, args, str);
	} else if (_wcsicmp(command, _T("getVariable")) == 0) {
		setVariable(argLen, args, str);
	} else if (_wcsicmp(command, _T("ini2reg")) == 0) {
		std::wcout << _T("Migrating to registry settings...")<< std::endl;
		NSCModuleHelper::ReadSettings(NSCAPI::settings_inifile);
		NSCModuleHelper::SetSettingsInt(MAIN_SECTION_TITLE, MAIN_USEFILE, 0);
		NSCModuleHelper::WriteSettings(NSCAPI::settings_inifile);
		NSCModuleHelper::SetSettingsInt(MAIN_SECTION_TITLE, MAIN_USEREG, 1);
		NSCModuleHelper::WriteSettings(NSCAPI::settings_registry);
	} else if (_wcsicmp(command, _T("reg2ini")) == 0) {
		std::wcout << _T("Migrating to INI file settings...")<< std::endl;
		NSCModuleHelper::ReadSettings(NSCAPI::settings_registry);
		NSCModuleHelper::SetSettingsInt(MAIN_SECTION_TITLE, MAIN_USEREG, 0);
		NSCModuleHelper::WriteSettings(NSCAPI::settings_registry);
		NSCModuleHelper::SetSettingsInt(MAIN_SECTION_TITLE, MAIN_USEFILE, 1);
		NSCModuleHelper::WriteSettings(NSCAPI::settings_inifile);
	}
	return 0;
}


NSCAPI::nagiosReturn RemoteConfiguration::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("setVariable")) {
		setVariable(argLen, char_args, msg);
		return NSCAPI::returnOK;
	} else if (command == _T("getVariable")) {
		getVariable(argLen, char_args, msg);
		return NSCAPI::returnOK;
	} else if (command == _T("readConf")) {
		return readConf(argLen, char_args, msg);
	} else if (command == _T("writeConf")) {
		return writeConf(argLen, char_args, msg);
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gRemoteConfiguration);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gRemoteConfiguration);
NSC_WRAPPERS_CLI_DEF(gRemoteConfiguration);

