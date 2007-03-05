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
NSCAPI::nagiosReturn RemoteConfiguration::writeConf(const unsigned int argLen, char **char_args, std::string &message) {
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	if (args.size() > 0) {
		if (args.front() == "reg") {
			if (NSCModuleHelper::WriteSettings(NSCAPI::settings_registry) == NSCAPI::isSuccess) {
				message = "Settings written successfully.";
				return NSCAPI::returnOK;
			}
			message = "ERROR could not write settings.";
			return NSCAPI::returnCRIT;
		}
	}
	if (NSCModuleHelper::WriteSettings(NSCAPI::settings_inifile) == NSCAPI::isSuccess) {
		message = "Settings written successfully.";
		return NSCAPI::returnOK;
	}
	message = "ERROR could not write settings.";
	return NSCAPI::returnCRIT;
}

NSCAPI::nagiosReturn RemoteConfiguration::readConf(const unsigned int argLen, char **char_args, std::string &message) {
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	if (args.size() > 0) {
		if (args.front() == "reg") {
			if (NSCModuleHelper::ReadSettings(NSCAPI::settings_registry) == NSCAPI::isSuccess) {
				message = "Settings written successfully.";
				return NSCAPI::returnOK;
			}
			message = "ERROR could not write settings.";
			return NSCAPI::returnCRIT;
		}
	}
	if (NSCModuleHelper::ReadSettings(NSCAPI::settings_inifile) == NSCAPI::isSuccess) {
		message = "Settings written successfully.";
		return NSCAPI::returnOK;
	}
	message = "ERROR could not write settings.";
	return NSCAPI::returnCRIT;
}
// set setVariable int <section> <variable> <value>
NSCAPI::nagiosReturn RemoteConfiguration::setVariable(const unsigned int argLen, char **char_args, std::string &message) {
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.size() < 3) {
		message = "Invalid syntax.";
		return NSCAPI::returnUNKNOWN;
	}
	std::string type = args.front(); args.pop_front();
	std::string section = args.front(); args.pop_front();
	std::string key = args.front(); args.pop_front();
	std::string value;
	if (args.size() >= 1) {
		value = args.front();
	}
	if (type == "int") {
		NSCModuleHelper::SetSettingsInt(section, key, strEx::stoi(value));
		message = "Settings " + key + " saved successfully.";
		return NSCAPI::returnOK;
	} else if (type == "string") {
		NSCModuleHelper::SetSettingsString(section, key, value);
		message = "Settings " + key + " saved successfully.";
		return NSCAPI::returnOK;
	} else {
		NSCModuleHelper::SetSettingsString(type, section, key);
		message = "Settings " + section + " saved successfully.";
		return NSCAPI::returnOK;
	}
}
NSCAPI::nagiosReturn RemoteConfiguration::getVariable(const unsigned int argLen, char **char_args, std::string &message) {
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.size() < 2) {
		message = "Invalid syntax.";
		return NSCAPI::returnUNKNOWN;
	}
	std::string section = args.front(); args.pop_front();
	std::string key = args.front(); args.pop_front();
	std::string value;
	value = NSCModuleHelper::getSettingsString(section, key, "");
	message = section+"/"+key+"="+value;
	return NSCAPI::returnOK;
}
int RemoteConfiguration::commandLineExec(const char* command,const unsigned int argLen,char** args) {
	std::string str;
	if (_stricmp(command, "setVariable") == 0) {
		setVariable(argLen, args, str);
	} else if (_stricmp(command, "writeConf") == 0) {
		writeConf(argLen, args, str);
	} else if (_stricmp(command, "getVariable") == 0) {
		setVariable(argLen, args, str);
	} else if (_stricmp(command, "ini2reg") == 0) {
		std::cout << "Migrating to registry settings..."<< std::endl;
		NSCModuleHelper::ReadSettings(NSCAPI::settings_inifile);
		NSCModuleHelper::SetSettingsInt(MAIN_SECTION_TITLE, MAIN_USEFILE, 0);
		NSCModuleHelper::WriteSettings(NSCAPI::settings_inifile);
		NSCModuleHelper::SetSettingsInt(MAIN_SECTION_TITLE, MAIN_USEREG, 1);
		NSCModuleHelper::WriteSettings(NSCAPI::settings_registry);
	} else if (_stricmp(command, "reg2ini") == 0) {
		std::cout << "Migrating to INI file settings..."<< std::endl;
		NSCModuleHelper::ReadSettings(NSCAPI::settings_registry);
		NSCModuleHelper::SetSettingsInt(MAIN_SECTION_TITLE, MAIN_USEREG, 0);
		NSCModuleHelper::WriteSettings(NSCAPI::settings_registry);
		NSCModuleHelper::SetSettingsInt(MAIN_SECTION_TITLE, MAIN_USEFILE, 1);
		NSCModuleHelper::WriteSettings(NSCAPI::settings_inifile);
	}
	return 0;
}


NSCAPI::nagiosReturn RemoteConfiguration::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) {
	if (command == "setVariable") {
		setVariable(argLen, char_args, msg);
		return NSCAPI::returnOK;
	} else if (command == "getVariable") {
		getVariable(argLen, char_args, msg);
		return NSCAPI::returnOK;
	} else if (command == "readConf") {
		return readConf(argLen, char_args, msg);
	} else if (command == "writeConf") {
		return writeConf(argLen, char_args, msg);
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gRemoteConfiguration);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gRemoteConfiguration);
NSC_WRAPPERS_CLI_DEF(gRemoteConfiguration);

