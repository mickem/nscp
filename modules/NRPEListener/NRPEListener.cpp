// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "NRPEListener.h"
#include <strEx.h>
#include <time.h>
#include <config.h>
#include "NRPEPacket.h"

NRPEListener gNRPEListener;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

NRPEListener::NRPEListener() {
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

bool NRPEListener::loadModule() {
	bUseSSL_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_USE_SSL ,NRPE_SETTINGS_USE_SSL_DEFAULT)==1;
	timeout = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_TIMEOUT ,NRPE_SETTINGS_TIMEOUT_DEFAULT);
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
			addCommand(command_name.c_str(), s);
		}
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
	commandList::iterator it = commands.find(command);
	if (it == commands.end())
		return NSCAPI::returnIgnored;

	std::string str = (*it).second;
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 1) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit = arr.begin();
		int i=1;

		for (;cit!=arr.end();cit++,i++) {
			if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
				if ((*cit).find_first_of(NASTY_METACHARS) != std::string::npos) {
					NSC_LOG_ERROR("Request string contained illegal metachars!");
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(str, "$ARG" + strEx::itos(i) + "$", (*cit));
		}
	}

	if ((str.substr(0,6) == "inject")&&(str.length() > 7)) {
		strEx::token t = strEx::getToken(str.substr(7), ' ');
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

			} else {
				pEnd = p = s.find(' ', ++p);
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
			p++;
		}
		return NSCModuleHelper::InjectSplitAndCommand(t.first, sTarget, '!', message, perf);
	}

	return executeNRPECommand(str, message, perf);
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
	strncpy(cmd, command.c_str(), command.length());
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
			msg = "The check didn't respond within the timeout period!";
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
	if (!allowedHosts.inAllowedHosts(client->getAddrString())) {
		NSC_LOG_ERROR("Unothorized access from: " + client->getAddrString());
		client->close();
		return;
	}
	try {
		simpleSocket::DataBuffer block;

		for (int i=0;i<100;i++) {
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
	std::string msg, perf;

	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 0) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR("Request contained arguments (not currently allowed).");
			throw NRPEException("Request contained arguments (not currently allowed).");
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
	if (perf.empty()) {
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
