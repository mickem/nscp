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
#include "NRPEListener.h"
#include <strEx.h>
#include <time.h>
#include <config.h>
#include "NRPEPacket.h"
#include <msvc_wrappers.h>

NRPEListener gNRPEListener;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

NRPEListener::NRPEListener() : noPerfData_(false), buffer_length_(0) {
}
NRPEListener::~NRPEListener() {
}

std::wstring getAllowedHosts() {
	std::wstring ret = NSCModuleHelper::getSettingsString(NRPE_SECTION_TITLE, MAIN_ALLOWED_HOSTS, _T(""));
	if (ret.empty())
		ret = NSCModuleHelper::getSettingsString(MAIN_SECTION_TITLE, MAIN_ALLOWED_HOSTS, MAIN_ALLOWED_HOSTS_DEFAULT);
	return ret;
}
bool getCacheAllowedHosts() {
	int val = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, MAIN_ALLOWED_HOSTS_CACHE, -1);
	if (val == -1)
		val = NSCModuleHelper::getSettingsInt(MAIN_SECTION_TITLE, MAIN_ALLOWED_HOSTS_CACHE, MAIN_ALLOWED_HOSTS_CACHE_DEFAULT);
	return val==1?true:false;
}


void NRPEListener::addAllScriptsFrom(std::wstring path) {
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
				addCommand(script_dir, wfd.cFileName);
			}
		} while (FindNextFile(hFind, &wfd));
	} else {
		NSC_LOG_ERROR_STD(_T("No scripts found in path: ") + path);
		return;
	}
	FindClose(hFind);
}

bool NRPEListener::loadModule() {
	bUseSSL_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_USE_SSL ,NRPE_SETTINGS_USE_SSL_DEFAULT)==1;
	noPerfData_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_PERFDATA,NRPE_SETTINGS_PERFDATA_DEFAULT)==0;
	timeout = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_TIMEOUT ,NRPE_SETTINGS_TIMEOUT_DEFAULT);
	socketTimeout_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_READ_TIMEOUT ,NRPE_SETTINGS_READ_TIMEOUT_DEFAULT);
	scriptDirectory_ = NSCModuleHelper::getSettingsString(NRPE_SECTION_TITLE, NRPE_SETTINGS_SCRIPTDIR ,NRPE_SETTINGS_SCRIPTDIR_DEFAULT);
	buffer_length_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_STRLEN, NRPE_SETTINGS_STRLEN_DEFAULT);
	std::list<std::wstring> commands = NSCModuleHelper::getSettingsSection(NRPE_HANDLER_SECTION_TITLE);
	std::list<std::wstring>::const_iterator it;
	for (it = commands.begin(); it != commands.end(); ++it) {
		std::wstring command_name;
		if (((*it).length() > 7)&&((*it).substr(0,7) == _T("command"))) {
			strEx::token t = strEx::getToken((*it), '[');
			t = strEx::getToken(t.second, ']');
			command_name = t.first;
		} else {
			command_name = (*it);
		}
		std::wstring s = NSCModuleHelper::getSettingsString(NRPE_HANDLER_SECTION_TITLE, (*it), _T(""));
		if (command_name.empty() || s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid command definition: ") + (*it));
		} else {
			if ((s.length() > 7)&&(s.substr(0,6) == _T("inject"))) {
				addCommand(inject, command_name.c_str(), s.substr(7));
			} else {
				addCommand(script, command_name.c_str(), s);
			}
		}
	}

	if (!scriptDirectory_.empty()) {
		addAllScriptsFrom(scriptDirectory_);
	}

	allowedHosts.setAllowedHosts(strEx::splitEx(getAllowedHosts(), _T(",")), getCacheAllowedHosts());
	try {
		unsigned short port = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_PORT, NRPE_SETTINGS_PORT_DEFAULT);
		std::wstring host = NSCModuleHelper::getSettingsString(NRPE_SECTION_TITLE, NRPE_SETTINGS_BINDADDR, NRPE_SETTINGS_BINDADDR_DEFAULT);
		unsigned int backLog = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_LISTENQUE, NRPE_SETTINGS_LISTENQUE_DEFAULT);
		if (bUseSSL_) {
			socket_ssl_.setHandler(this);
			socket_ssl_.StartListener(host, port, backLog);
		} else {
			socket_.setHandler(this);
			socket_.StartListener(host, port, backLog);
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
	} catch (simpleSSL::SSLException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
	}

	return true;
}
bool NRPEListener::unloadModule() {
	try {
		if (bUseSSL_) {
			socket_ssl_.removeHandler(this);
			if (socket_ssl_.hasListener())
				socket_ssl_.StopListener();
		} else {
			socket_.removeHandler(this);
			if (socket_.hasListener())
				socket_.StopListener();
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
	} catch (simpleSSL::SSLException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
	}
	return true;
}


bool NRPEListener::hasCommandHandler() {
	return true;
}
bool NRPEListener::hasMessageHandler() {
	return false;
}


NSCAPI::nagiosReturn NRPEListener::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	command_list::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	const command_data cd = (*cit).second;
	std::wstring args = cd.arguments;
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 1) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit2 = arr.begin();
		int i=1;

		for (;cit2!=arr.end();cit2++,i++) {
			if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
				if ((*cit2).find_first_of(NASTY_METACHARS) != std::wstring::npos) {
					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, _T("$ARG") + strEx::itos(i) + _T("$"), (*cit2));
		}
	}
	if (cd.type == inject) {
		strEx::token t = strEx::getToken(args, ' ');
		std::wstring s = t.second;
		std::wstring sTarget;

		std::wstring::size_type p = 0;
		while(true) {
			std::wstring::size_type pStart = p;
			std::wstring::size_type pEnd = std::wstring::npos;
			if (s[p] == '\"') {
				pStart++;
				while (true) {
					p = s.find(' ', ++p);
					if (p == std::wstring::npos)
						break;
					if ((p>1)&&(s[p-1]=='\"')&&(((p>2)&&(s[p-2]!='\\'))||(p==2)))
						break;
				}
				if (p != std::wstring::npos)
					pEnd = p-1;
				else
					pEnd = s.length()-1;
				if (p != std::wstring::npos) {
					p++;
				}
			} else {
				pEnd = p = s.find(' ', ++p);
				if (p != std::wstring::npos) {
					p = s.find_first_not_of(' ', p);
				}
			}
			if (!sTarget.empty())
				sTarget += _T("!");
			if (p == std::wstring::npos) {
				if (pEnd == std::wstring::npos)
					sTarget += s.substr(pStart);
				else
					sTarget += s.substr(pStart, pEnd-pStart);
				break;
			}
			sTarget += s.substr(pStart,pEnd-pStart);
			//p++;
		}
		return NSCModuleHelper::InjectSplitAndCommand(t.first, sTarget, '!', message, perf);
	} else if (cd.type == script) {
		return executeNRPECommand(args, message, perf);
	} else if (cd.type == script_dir) {
		std::wstring args = arrayBuffer::arrayBuffer2string(char_args, argLen, _T(" "));
		std::wstring cmd = scriptDirectory_ + command.c_str() + _T(" ") +args;
		return executeNRPECommand(cmd, message, perf);
	} else {
		NSC_LOG_ERROR_STD(_T("Unknown script type: ") + command.c_str());
		return NSCAPI::critical;
	}

}
int NRPEListener::executeNRPECommand(std::wstring command, std::wstring &msg, std::wstring &perf)
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

	// Create Pipes
	CreatePipe(&hChildInR, &hChildInW, &sec, 0);
	CreatePipe(&hChildOutR, &hChildOutW, &sec, 0);

	// Set up members of STARTUPINFO structure. 

	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
	si.hStdInput = hChildInR;
	si.hStdOutput = hChildOutW;
	si.hStdError = hChildOutW;
	si.wShowWindow = SW_HIDE;


	// CreateProcess doesn't work with a const command
	TCHAR *cmd = new TCHAR[command.length()+1];
	wcsncpy_s(cmd, command.length()+1, command.c_str(), command.length());
	cmd[command.length()] = 0;
	std::wstring root = NSCModuleHelper::getBasePath();

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
			std::string str;
#define BUFF_SIZE 4096
			char *buf = new char[BUFF_SIZE+1];
			do {
				retval = ReadFile(hChildOutR, buf, BUFF_SIZE, &dwread, NULL);
				if (retval == 0)
					break;
				if (dwread > BUFF_SIZE)
					break;
				buf[dwread] = 0;
				str += buf;
			} while (dwread == BUFF_SIZE);
			delete [] buf;
			if (str.empty()) {
				msg = _T("No output available from command...");
			} else {
				msg = strEx::string_to_wstring(str);
				strEx::token t = strEx::getToken(msg, '|');
				msg = t.first;
				std::wstring::size_type pos = msg.find_last_not_of(_T("\n\r "));
				if (pos != std::wstring::npos) {
					if (pos == msg.size())
						msg = msg.substr(0,pos);
					else
						msg = msg.substr(0,pos+1);
				}
				perf = t.second;
			}
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
		msg = _T("NRPE_NT failed to create process (") + command + _T("): ") + error::lookup::last_error();
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
void NRPEListener::onClose()
{}

void NRPEListener::onAccept(simpleSocket::Socket *client) 
{
	if (!allowedHosts.inAllowedHosts(client->getAddr())) {
		NSC_LOG_ERROR(_T("Unauthorize access from: ") + client->getAddrString());
		client->close();
		return;
	}
	try {
		simpleSocket::DataBuffer block;
		int i;
		int maxWait = socketTimeout_*10;
		for (i=0;i<maxWait;i++) {
			bool lastReadHasMore = false;
			try {
				lastReadHasMore = client->readAll(block, 1048);
			} catch (simpleSocket::SocketException e) {
				NSC_LOG_MESSAGE(_T("Could not read NRPE packet from socket :") + e.getMessage());
				client->close();
				return;
			}
			if (block.getLength() >= NRPEPacket::getBufferLength(buffer_length_))
				break;
			if (!lastReadHasMore) {
				client->close();
				return;
			}
			Sleep(100);
		}
		if (i >= maxWait) {
			NSC_LOG_ERROR_STD(_T("Timeout reading NRPE-packet (increase socket_timeout)"));
			client->close();
			return;
		}
		if (block.getLength() == NRPEPacket::getBufferLength(buffer_length_)) {
			try {
				NRPEPacket out = handlePacket(NRPEPacket(block.getBuffer(), block.getLength(), buffer_length_));
				block.copyFrom(out.getBuffer(), out.getBufferLength());
			} catch (NRPEPacket::NRPEPacketException e) {
				NSC_LOG_ERROR_STD(_T("NRPESocketException: ") + e.getMessage());
				client->close();
				return;
			}
			client->send(block);
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD(_T("SocketException: ") + e.getMessage());
	} catch (NRPEException e) {
		NSC_LOG_ERROR_STD(_T("NRPEException: ") + e.getMessage());
	}
	client->close();
}

NRPEPacket NRPEListener::handlePacket(NRPEPacket p) {
	if (p.getType() != NRPEPacket::queryPacket) {
		NSC_LOG_ERROR(_T("Request is not a query."));
		throw NRPEException(_T("Invalid query type"));
	}
	if (p.getVersion() != NRPEPacket::version2) {
		NSC_LOG_ERROR(_T("Request had unsupported version."));
		throw NRPEException(_T("Invalid version"));
	}
	if (!p.verifyCRC()) {
		NSC_LOG_ERROR(_T("Request had invalid checksum."));
		throw NRPEException(_T("Invalid checksum"));
	}
	strEx::token cmd = strEx::getToken(p.getPayload(), '!');
	if (cmd.first == _T("_NRPE_CHECK")) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnOK, _T("I (") SZVERSION _T(") seem to be doing fine..."), buffer_length_);
	}
	std::wstring msg, perf;

	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 0) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
			throw NRPEException(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
		}
	}
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
			throw NRPEException(_T("Request command contained illegal metachars!"));
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
			throw NRPEException(_T("Request command contained illegal metachars!"));
		}
	}

	NSCAPI::nagiosReturn ret = -3;
	try {
		ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first, cmd.second, '!', msg, perf);
	} catch (...) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("UNKNOWN: Internal exception"), buffer_length_);
	}
	switch (ret) {
		case NSCAPI::returnInvalidBufferLen:
			msg = _T("UNKNOWN: Return buffer to small to handle this command.");
			ret = NSCAPI::returnUNKNOWN;
			break;
		case NSCAPI::returnIgnored:
			msg = _T("UNKNOWN: No handler for that command");
			ret = NSCAPI::returnUNKNOWN;
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnWARN:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnUNKNOWN:
			break;
		default:
			msg = _T("UNKNOWN: Internal error.");
			ret = NSCAPI::returnUNKNOWN;
	}
	if (msg.length() > buffer_length_) {
		NSC_LOG_ERROR(_T("Truncating returndata as it is bigger then NRPE allowes :("));
		msg = msg.substr(0,buffer_length_-1);
	}
	if (perf.empty()||noPerfData_) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg, buffer_length_);
	} else {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg + _T("|") + perf, buffer_length_);
	}
}

NSC_WRAPPERS_MAIN_DEF(gNRPEListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEListener);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNRPEListener);


MODULE_SETTINGS_START(NRPEListener, _T("NRPE Listener configuration"), _T("...")) 

PAGE(_T("NRPE Listsner configuration")) 

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the NRPEListener.dll will listen to.")) 
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
