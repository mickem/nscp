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
#include "CheckExternalScripts.h"
#include <strEx.h>
#include <time.h>
#include <config.h>
#include <msvc_wrappers.h>

CheckExternalScripts gCheckExternalScripts;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

CheckExternalScripts::CheckExternalScripts() {}
CheckExternalScripts::~CheckExternalScripts() {}

void CheckExternalScripts::addAllScriptsFrom(std::wstring path) {
	std::wstring baseDir;
	std::wstring::size_type pos = path.find_last_of('*');
	if (pos == std::wstring::npos) {
		path += _T("*.*");
	}
	WIN32_FIND_DATA wfd;
	HANDLE hFind = FindFirstFile(path.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if ((wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
				addCommand(wfd.cFileName);
			}
		} while (FindNextFile(hFind, &wfd));
	} else {
		NSC_LOG_ERROR_STD(_T("No scripts found in path: ") + path);
		return;
	}
	FindClose(hFind);
}

bool CheckExternalScripts::loadModule() {
	timeout = NSCModuleHelper::getSettingsInt(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_TIMEOUT ,EXTSCRIPT_SETTINGS_TIMEOUT_DEFAULT);
	scriptDirectory_ = NSCModuleHelper::getSettingsString(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_SCRIPTDIR ,EXTSCRIPT_SETTINGS_SCRIPTDIR_DEFAULT);
	std::list<std::wstring>::const_iterator it;
	std::list<std::wstring> commands = NSCModuleHelper::getSettingsSection(EXTSCRIPT_SCRIPT_SECTION_TITLE);
	for (it = commands.begin(); it != commands.end(); ++it) {
		if ((*it).empty())
			continue;
		std::wstring s = NSCModuleHelper::getSettingsString(EXTSCRIPT_SCRIPT_SECTION_TITLE, (*it), _T(""));
		if (s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid command definition: ") + (*it));
		} else {
			strEx::token tok = strEx::getToken(s, ' ', true);
			addCommand((*it).c_str(), tok.first, tok.second);
		}
	}

	commands = NSCModuleHelper::getSettingsSection(EXTSCRIPT_ALIAS_SECTION_TITLE);
	for (it = commands.begin(); it != commands.end(); ++it) {
		if ((*it).empty())
			continue;
		std::wstring s = NSCModuleHelper::getSettingsString(EXTSCRIPT_ALIAS_SECTION_TITLE, (*it), _T(""));
		if (s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid command definition: ") + (*it));
		} else {
			strEx::token tok = strEx::getToken(s, ' ', true);
			addAlias((*it).c_str(), tok.first, tok.second);
		}
	}

	if (!scriptDirectory_.empty()) {
		addAllScriptsFrom(scriptDirectory_);
	}
	return true;
}
bool CheckExternalScripts::unloadModule() {
	return true;
}


bool CheckExternalScripts::hasCommandHandler() {
	return true;
}
bool CheckExternalScripts::hasMessageHandler() {
	return false;
}


NSCAPI::nagiosReturn CheckExternalScripts::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	command_list::const_iterator cit = commands.find(command);
	bool isAlias = false;
	if (cit == commands.end()) {
		cit = alias.find(command);
		if (cit == alias.end())
			return NSCAPI::returnIgnored;
		isAlias = true;
	}

	const command_data cd = (*cit).second;
	std::wstring args = cd.arguments;
	if (isAlias || NSCModuleHelper::getSettingsInt(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_ALLOW_ARGUMENTS, EXTSCRIPT_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 1) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit2 = arr.begin();
		int i=1;

		for (;cit2!=arr.end();cit2++,i++) {
			if (isAlias || NSCModuleHelper::getSettingsInt(EXTSCRIPT_SECTION_TITLE, EXTSCRIPT_SETTINGS_ALLOW_NASTY_META, EXTSCRIPT_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
				if ((*cit2).find_first_of(NASTY_METACHARS) != std::wstring::npos) {
					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, _T("$ARG") + strEx::itos(i) + _T("$"), (*cit2));
		}
	}
	if (isAlias) {
		return NSCModuleHelper::InjectSplitAndCommand(cd.command, cd.arguments, ' ', message, perf, true);
	} else {
		return executeNRPECommand(cd.command + _T(" ") + args, message, perf);
		/*
	} else if (cd.type == script_dir) {
		std::wstring args = arrayBuffer::arrayBuffer2string(char_args, argLen, _T(" "));
		std::wstring cmd = scriptDirectory_ + command.c_str() + _T(" ") +args;
		return executeNRPECommand(cmd, message, perf);
	} else {
		NSC_LOG_ERROR_STD(_T("Unknown script type: ") + command.c_str());
		return NSCAPI::critical;
		*/
	}

}
#define MAX_INPUT_BUFFER 1024

int CheckExternalScripts::executeNRPECommand(std::wstring command, std::wstring &msg, std::wstring &perf)
{
	NSCAPI::nagiosReturn result;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	HANDLE hChildOutR, hChildOutW, hChildInR, hChildInW;
	SECURITY_ATTRIBUTES sec;
	DWORD dwstate, dwexitcode;
	int retval;


	// Set up members of SECURITY_ATTRIBUTES structure. 

	sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	sec.bInheritHandle = TRUE;
	sec.lpSecurityDescriptor = NULL;

	// CreateProcess doesn't work with a const command
	TCHAR *cmd = new TCHAR[command.length()+1];
	if (cmd == NULL) {
		NSC_LOG_ERROR(_T("Failed to allocate memory for command buffer (") + command + _T(")."));
		return NSCAPI::returnUNKNOWN;
	}
	wcsncpy_s(cmd, command.length()+1, command.c_str(), command.length());
	cmd[command.length()] = 0;
	std::wstring root = NSCModuleHelper::getBasePath();

	// Create Pipes
	if (!CreatePipe(&hChildInR, &hChildInW, &sec, 0)) {
		NSC_LOG_ERROR(_T("Failed to create pipe for (") + command + _T(") return code: ") + error::lookup::last_error());
		return NSCAPI::returnUNKNOWN;
	}
	if (!CreatePipe(&hChildOutR, &hChildOutW, &sec, 0)) {
		NSC_LOG_ERROR(_T("Failed to create pipe for (") + command + _T(") return code: ") + error::lookup::last_error());
		return NSCAPI::returnUNKNOWN;
	}

	// Set up members of STARTUPINFO structure. 

	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
	si.hStdInput = hChildInR;
	si.hStdOutput = hChildOutW;
	si.hStdError = hChildOutW;
	si.wShowWindow = SW_HIDE;

	// Create the child process. 
	BOOL processOK = CreateProcess(NULL, cmd,        // command line 
		NULL, // process security attributes 
		NULL, // primary thread security attributes 
		TRUE, // handles are inherited 
		0,    // creation flags 
		NULL, // use parent's environment 
		root.c_str(), // use parent's current directory 
		&si,  // STARTUPINFO pointer 
		&pi); // receives PROCESS_INFORMATION 
	delete [] cmd;

	if (processOK) {
		dwstate = WaitForSingleObject(pi.hProcess, 1000*timeout);
		CloseHandle(hChildInR);
		CloseHandle(hChildInW);
		CloseHandle(hChildOutW);

		if (dwstate == WAIT_TIMEOUT) {
			TerminateProcess(pi.hProcess, 5);
			msg = _T("The check (") + command + _T(") didn't respond within the timeout period (") + strEx::itos(timeout) + _T("s)!");
			result = NSCAPI::returnUNKNOWN;
		} else {
			DWORD dwread;
			//TCHAR *buf = new TCHAR[MAX_INPUT_BUFFER+1];
			char *buf = new char[MAX_INPUT_BUFFER+1];
			//retval = ReadFile(hChildOutR, buf, MAX_INPUT_BUFFER*sizeof(WCHAR), &dwread, NULL);
			retval = ReadFile(hChildOutR, buf, MAX_INPUT_BUFFER*sizeof(char), &dwread, NULL);
			if (!retval || dwread == 0) {
				msg = _T("No output available from command...");
			} else {
				buf[dwread] = 0;
				msg = strEx::string_to_wstring(buf);
				//msg = buf;
				//strEx::token t = strEx::getToken(msg, '\n');
				strEx::token t = strEx::getToken(msg, '|');
				msg = t.first;
				std::wstring::size_type pos = msg.find_last_not_of(_T("\n\r "));
				if (pos != std::wstring::npos) {
					if (pos == msg.size())
						msg = msg.substr(0,pos);
					else
						msg = msg.substr(0,pos+1);
				}
				//if (msg[msg.size()-1] == '\n')
				perf = t.second;
			}
			delete [] buf;
			if (GetExitCodeProcess(pi.hProcess, &dwexitcode) == 0) {
				NSC_LOG_ERROR(_T("Failed to get commands (") + command + _T(") return code: ") + error::lookup::last_error());
				dwexitcode = NSCAPI::returnUNKNOWN;
			}
			if (!NSCHelper::isNagiosReturnCode(dwexitcode)) {
				NSC_LOG_ERROR(_T("The command (") + command + _T(") returned an invalid return code: ") + strEx::itos(dwexitcode));
				dwexitcode = NSCAPI::returnUNKNOWN;
			}
			result = NSCHelper::int2nagios(dwexitcode);
		}
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		CloseHandle(hChildOutR);
	}
	else {
		msg = _T("NSCP failed to create process (") + command + _T("): ") + error::lookup::last_error();
		result = NSCAPI::returnUNKNOWN;
		CloseHandle(hChildInR);
		CloseHandle(hChildInW);
		CloseHandle(hChildOutW);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		CloseHandle(hChildOutR);
	}
	return result;
}


NSC_WRAPPERS_MAIN_DEF(gCheckExternalScripts);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckExternalScripts);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gCheckExternalScripts);


MODULE_SETTINGS_START(CheckExternalScripts, _T("NRPE Listener configuration"), _T("...")) 

PAGE(_T("NRPE Listsner configuration")) 

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the CheckExternalScripts.dll will listen to.")) 
ITEM_MAP_TO(_T("basic_ini_text_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("port")) 
OPTION(_T("default"), _T("5666")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("allow_arguments"), _T("This option determines whether or not the NRPE daemon will allow clients to specify arguments to commands that are executed.")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allow_arguments")) 
OPTION(_T("default"), _T("false")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("allow_nasty_meta_chars"), _T("This might have security implications (depending on what you do with the options)")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allow_nasty_meta_chars")) 
OPTION(_T("default"), _T("false")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("use_ssl"), _T("This option will enable SSL encryption on the NRPE data socket (this increases security somwhat.")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("use_ssl")) 
OPTION(_T("default"), _T("true")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

PAGE_END()
ADVANCED_PAGE(_T("Access configuration")) 

ITEM_EDIT_OPTIONAL_LIST(_T("Allow connection from:"), _T("This is the hosts that will be allowed to poll performance data from the NRPE server.")) 
OPTION(_T("disabledCaption"), _T("Use global settings (defined previously)")) 
OPTION(_T("enabledCaption"), _T("Specify hosts for NRPE server")) 
OPTION(_T("listCaption"), _T("Add all IP addresses (not hosts) which should be able to connect:")) 
OPTION(_T("separator"), _T(",")) 
OPTION(_T("disabled"), _T("")) 
ITEM_MAP_TO(_T("basic_ini_text_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allowed_hosts")) 
OPTION(_T("default"), _T("")) 
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()
