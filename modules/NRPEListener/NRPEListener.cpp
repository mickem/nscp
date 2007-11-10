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

NRPEListener::NRPEListener() : noPerfData_(false) {
}
NRPEListener::~NRPEListener() {
}

std::string getAllowedHosts() {
	std::string ret = NSCModuleHelper::getSettingsString(NRPE_SECTION_TITLE, MAIN_ALLOWED_HOSTS, "");
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


void NRPEListener::addAllScriptsFrom(std::string path) {
	std::string baseDir;
	std::string::size_type pos = path.find_last_of('*');
	if (pos == std::string::npos) {
		path += "*.*";
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
		NSC_LOG_ERROR_STD("No scripts found in path: " + path);
		return;
	}
	FindClose(hFind);
}

bool NRPEListener::loadModule() {
	bUseSSL_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_USE_SSL ,NRPE_SETTINGS_USE_SSL_DEFAULT)==1;
	noPerfData_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_PERFDATA,NRPE_SETTINGS_PERFDATA_DEFAULT)==0;
	timeout = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_TIMEOUT ,NRPE_SETTINGS_TIMEOUT_DEFAULT);
	scriptDirectory_ = NSCModuleHelper::getSettingsString(NRPE_SECTION_TITLE, NRPE_SETTINGS_SCRIPTDIR ,NRPE_SETTINGS_SCRIPTDIR_DEFAULT);
	std::list<std::string> commands = NSCModuleHelper::getSettingsSection(NRPE_HANDLER_SECTION_TITLE);
	std::list<std::string>::const_iterator it;
	for (it = commands.begin(); it != commands.end(); ++it) {
		std::string command_name;
		if (((*it).length() > 7)&&((*it).substr(0,7) == "command")) {
			strEx::token t = strEx::getToken((*it), '[');
			t = strEx::getToken(t.second, ']');
			command_name = t.first;
		} else {
			command_name = (*it);
		}
		std::string s = NSCModuleHelper::getSettingsString(NRPE_HANDLER_SECTION_TITLE, (*it), "");
		if (command_name.empty() || s.empty()) {
			NSC_LOG_ERROR_STD("Invalid command definition: " + (*it));
		} else {
			if ((s.length() > 7)&&(s.substr(0,6) == "inject")) {
				addCommand(inject, command_name.c_str(), s.substr(7));
			} else {
				addCommand(script, command_name.c_str(), s);
			}
		}
	}

	if (!scriptDirectory_.empty()) {
		addAllScriptsFrom(scriptDirectory_);
	}

	allowedHosts.setAllowedHosts(strEx::splitEx(getAllowedHosts(), ","), getCacheAllowedHosts());
	try {
		unsigned short port = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_PORT, NRPE_SETTINGS_PORT_DEFAULT);
		std::string host = NSCModuleHelper::getSettingsString(NRPE_SECTION_TITLE, NRPE_SETTINGS_BINDADDR, NRPE_SETTINGS_BINDADDR_DEFAULT);
		unsigned int backLog = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_LISTENQUE, NRPE_SETTINGS_LISTENQUE_DEFAULT);
		if (bUseSSL_) {
			socket_ssl_.setHandler(this);
			socket_ssl_.StartListener(host, port, backLog);
		} else {
			socket_.setHandler(this);
			socket_.StartListener(host, port, backLog);
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
		return false;
	} catch (simpleSSL::SSLException e) {
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
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
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
		return false;
	} catch (simpleSSL::SSLException e) {
		NSC_LOG_ERROR_STD("Exception caught: " + e.getMessage());
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


NSCAPI::nagiosReturn NRPEListener::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	command_list::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	const command_data cd = (*cit).second;
	std::string args = cd.arguments;
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 1) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit2 = arr.begin();
		int i=1;

		for (;cit2!=arr.end();cit2++,i++) {
			if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
				if ((*cit2).find_first_of(NASTY_METACHARS) != std::string::npos) {
					NSC_LOG_ERROR("Request string contained illegal metachars!");
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, "$ARG" + strEx::itos(i) + "$", (*cit2));
		}
	}
	if (cd.type == inject) {
		strEx::token t = strEx::getToken(args, ' ');
		std::string s = t.second;
		std::string sTarget;

		std::string::size_type p = 0;
		while(true) {
			std::string::size_type pStart = p;
			std::string::size_type pEnd = std::string::npos;
			if (s[p] == '\"') {
				pStart++;
				while (true) {
					p = s.find(' ', ++p);
					if (p == std::string::npos)
						break;
					if ((p>1)&&(s[p-1]=='\"')&&(((p>2)&&(s[p-2]!='\\'))||(p==2)))
						break;
				}
				if (p != std::string::npos)
					pEnd = p-1;
				else
					pEnd = s.length()-1;
				if (p != std::string::npos) {
					p++;
				}
			} else {
				pEnd = p = s.find(' ', ++p);
				if (p != std::string::npos) {
					p = s.find_first_not_of(' ', p);
				}
			}
			if (!sTarget.empty())
				sTarget += "!";
			if (p == std::string::npos) {
				if (pEnd == std::string::npos)
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
		std::string args = arrayBuffer::arrayBuffer2string(char_args, argLen, " ");
		std::string cmd = scriptDirectory_ + command.c_str() + " " +args;
		return executeNRPECommand(cmd, message, perf);
	} else {
		NSC_LOG_ERROR_STD("Unknown script type: " + command.c_str());
		return NSCAPI::critical;
	}

}
#define MAX_INPUT_BUFFER 1024

int NRPEListener::executeNRPECommand(std::string command, std::string &msg, std::string &perf)
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
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = hChildInR;
	si.hStdOutput = hChildOutW;
	si.hStdError = hChildOutW;


	// CreateProcess doesn't work with a const command
	char *cmd = new char[command.length()+1];
	strncpy_s(cmd, command.length()+1, command.c_str(), command.length());
	cmd[command.length()] = 0;

	// Create the child process. 
	BOOL processOK = CreateProcess(NULL, cmd,        // command line 
		NULL, // process security attributes 
		NULL, // primary thread security attributes 
		TRUE, // handles are inherited 
		0,    // creation flags 
		NULL, // use parent's environment 
		NULL, // use parent's current directory 
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
			msg = "The check (" + command + ") didn't respond within the timeout period (" + strEx::itos(timeout) + "s)!";
			result = NSCAPI::returnUNKNOWN;
		} else {
			DWORD dwread;
			char *buf = new char[MAX_INPUT_BUFFER+1];
			retval = ReadFile(hChildOutR, buf, MAX_INPUT_BUFFER, &dwread, NULL);
			if (!retval || dwread == 0) {
				msg = "No output available from command...";
			} else {
				buf[dwread] = 0;
				msg = buf;
				strEx::token t = strEx::getToken(msg, '\n');
				t = strEx::getToken(t.first, '|');
				msg = t.first;
				perf = t.second;
			}
			delete [] buf;
			GetExitCodeProcess(pi.hProcess, &dwexitcode);
			result = NSCHelper::int2nagios(dwexitcode);
		}
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		CloseHandle(hChildOutR);
	}
	else {
		msg = "NRPE_NT failed to create process, exiting...";
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
		NSC_LOG_ERROR("Unauthorize access from: " + client->getAddrString());
		client->close();
		return;
	}
	try {
		simpleSocket::DataBuffer block;
		int i;
		for (i=0;i<100;i++) {
			client->readAll(block, 1048);
			if (block.getLength() >= NRPEPacket::getBufferLength())
				break;
			Sleep(100);
		}
		if (i == 100) {
			NSC_LOG_ERROR_STD("Could not retrieve NRPE packet.");
			client->close();
			return;
		}
		if (block.getLength() == NRPEPacket::getBufferLength()) {
			try {
				NRPEPacket out = handlePacket(NRPEPacket(block.getBuffer(), block.getLength()));
				block.copyFrom(out.getBuffer(), out.getBufferLength());
			} catch (NRPEPacket::NRPEPacketException e) {
				NSC_LOG_ERROR_STD("NRPESocketException: " + e.getMessage());
				client->close();
				return;
			}
			client->send(block);
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD("SocketException: " + e.getMessage());
	} catch (NRPEException e) {
		NSC_LOG_ERROR_STD("NRPEException: " + e.getMessage());
	}
	client->close();
}

NRPEPacket NRPEListener::handlePacket(NRPEPacket p) {
	if (p.getType() != NRPEPacket::queryPacket) {
		NSC_LOG_ERROR("Request is not a query.");
		throw NRPEException("Invalid query type");
	}
	if (p.getVersion() != NRPEPacket::version2) {
		NSC_LOG_ERROR("Request had unsupported version.");
		throw NRPEException("Invalid version");
	}
	if (!p.verifyCRC()) {
		NSC_LOG_ERROR("Request had invalid checksum.");
		throw NRPEException("Invalid checksum");
	}
	strEx::token cmd = strEx::getToken(p.getPayload(), '!');
	if (cmd.first == "_NRPE_CHECK") {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnOK, "I ("SZVERSION") seem to be doing fine...");
	}
	std::string msg, perf;

	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 0) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR("Request contained arguments (not currently allowed, check the allow_arguments option).");
			throw NRPEException("Request contained arguments (not currently allowed, check the allow_arguments option).");
		}
	}
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::string::npos) {
			NSC_LOG_ERROR("Request command contained illegal metachars!");
			throw NRPEException("Request command contained illegal metachars!");
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::string::npos) {
			NSC_LOG_ERROR("Request arguments contained illegal metachars!");
			throw NRPEException("Request command contained illegal metachars!");
		}
	}

	NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first, cmd.second, '!', msg, perf);
	switch (ret) {
		case NSCAPI::returnInvalidBufferLen:
			msg = "UNKNOWN: Return buffer to small to handle this command.";
			ret = NSCAPI::returnUNKNOWN;
			break;
		case NSCAPI::returnIgnored:
			msg = "UNKNOWN: No handler for that command";
			ret = NSCAPI::returnUNKNOWN;
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnWARN:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnUNKNOWN:
			break;
		default:
			msg = "UNKNOWN: Internal error.";
			ret = NSCAPI::returnUNKNOWN;
	}
	if (perf.empty()||noPerfData_) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg);
	} else {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg + "|" + perf);
	}
}

NSC_WRAPPERS_MAIN_DEF(gNRPEListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEListener);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNRPEListener);


MODULE_SETTINGS_START(NRPEListener, "NRPE Listsner configuration", "...")

PAGE("NRPE Listsner configuration")

ITEM_EDIT_TEXT("port", "This is the port the NRPEListener.dll will listen to.")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "NRPE")
OPTION("key", "port")
OPTION("default", "5666")
ITEM_END()

ITEM_CHECK_BOOL("allow_arguments", "This option determines whether or not the NRPE daemon will allow clients to specify arguments to commands that are executed.")
ITEM_MAP_TO("basic_ini_bool_mapper")
OPTION("section", "NRPE")
OPTION("key", "allow_arguments")
OPTION("default", "false")
OPTION("true_value", "1")
OPTION("false_value", "0")
ITEM_END()

ITEM_CHECK_BOOL("allow_nasty_meta_chars", "This might have security implications (depending on what you do with the options)")
ITEM_MAP_TO("basic_ini_bool_mapper")
OPTION("section", "NRPE")
OPTION("key", "allow_nasty_meta_chars")
OPTION("default", "false")
OPTION("true_value", "1")
OPTION("false_value", "0")
ITEM_END()

ITEM_CHECK_BOOL("use_ssl", "This option will enable SSL encryption on the NRPE data socket (this increases security somwhat.")
ITEM_MAP_TO("basic_ini_bool_mapper")
OPTION("section", "NRPE")
OPTION("key", "use_ssl")
OPTION("default", "true")
OPTION("true_value", "1")
OPTION("false_value", "0")
ITEM_END()

PAGE_END()
ADVANCED_PAGE("Access configuration")

ITEM_EDIT_OPTIONAL_LIST("Allow connection from:", "This is the hosts that will be allowed to poll performance data from the NRPE server.")
OPTION("disabledCaption", "Use global settings (defined previously)")
OPTION("enabledCaption", "Specify hosts for NRPE server")
OPTION("listCaption", "Add all IP addresses (not hosts) which should be able to connect:")
OPTION("separator", ",")
OPTION("disabled", "")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "NRPE")
OPTION("key", "allowed_hosts")
OPTION("default", "")
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()
