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



bool NRPEListener::loadModule() {
	bUseSSL_ = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_USE_SSL ,NRPE_SETTINGS_USE_SSL_DEFAULT)==1;
	timeout = NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_TIMEOUT ,NRPE_SETTINGS_TIMEOUT_DEFAULT);
	std::list<std::string> commands = NSCModuleHelper::getSettingsSection(NRPE_HANDLER_SECTION_TITLE);
	std::list<std::string>::iterator it;
	for (it = commands.begin(); it != commands.end(); it++) {
		strEx::token t = strEx::getToken(*it, '=');
		if (t.first.substr(0,7) == "command") {
			strEx::token t2 = strEx::getToken(t.first, '[');
			t2 = strEx::getToken(t2.second, ']');
			t.first = t2.first;
		}
		if (t.first.empty() || t.second.empty()) {
			NSC_LOG_ERROR_STD("Invalid command definition: " + (*it));
		} else
			addCommand(t.first, t.second);
	}

	allowedHosts.setAllowedHosts(strEx::splitEx(NSCModuleHelper::getSettingsString(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOWED, NRPE_SETTINGS_ALLOWED_DEFAULT), ","));
	try {
		if (bUseSSL_) {
			socket_ssl_.setHandler(this);
			socket_ssl_.StartListener(NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_PORT, NRPE_SETTINGS_PORT_DEFAULT));
		} else {
			socket_.setHandler(this);
			socket_.StartListener(NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_PORT, NRPE_SETTINGS_PORT_DEFAULT));
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
			socket_ssl_.StopListener();
		} else {
			socket_.removeHandler(this);
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

std::string NRPEListener::getModuleName() {
	return "NRPE module.";
}
NSCModuleWrapper::module_version NRPEListener::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
}

bool NRPEListener::hasCommandHandler() {
	return true;
}
bool NRPEListener::hasMessageHandler() {
	return false;
}


NSCAPI::nagiosReturn NRPEListener::handleCommand(const std::string command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
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
			NSC_DEBUG_MSG_STD("Attempting to replace: " + "$ARG" + strEx::itos(i) + "$" + " with " + (*cit));
			strEx::replace(str, "$ARG" + strEx::itos(i) + "$", (*cit));
		}
	}

	if ((str.substr(0,6) == "inject")&&(str.length() > 7)) {
		strEx::token t = strEx::getToken(str.substr(7), ' ');
		NSC_DEBUG_MSG_STD("Injecting: " + t.first + ", " + t.second);
		return NSCModuleHelper::InjectSplitAndCommand(t.first, t.second, ' ', message, perf);
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
			result = NSCHelper::int2nagios(GetExitCodeProcess(pi.hProcess, &dwexitcode));
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

void NRPEListener::onAccept(simpleSocket::Socket &client) 
{
	if (!allowedHosts.inAllowedHosts(client.getAddrString())) {
		NSC_LOG_ERROR("Unothorized access from: " + client.getAddrString());
		client.close();
		return;
	}
	try {
		simpleSocket::DataBuffer block;

		for (int i=0;i<100;i++) {
			client.readAll(block);
			if (block.getLength() >= NRPEPacket::getBufferLength())
				break;
			Sleep(100);
		}
		if (i == 100) {
			NSC_LOG_ERROR_STD("Could not retrieve NRPE packet.");
			client.close();
			return;
		}

		if (block.getLength() == NRPEPacket::getBufferLength()) {
			try {
				NRPEPacket out = handlePacket(NRPEPacket(block.getBuffer(), block.getLength()));
				block.copyFrom(out.getBuffer(), out.getBufferLength());
			} catch (NRPEPacket::NRPEPacketException e) {
				NSC_LOG_ERROR_STD("NRPESocketException: " + e.getMessage());
				client.close();
				return;
			}
			client.send(block);
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD("SocketException: " + e.getMessage());
	} catch (NRPEException e) {
		NSC_LOG_ERROR_STD("NRPEException: " + e.getMessage());
	}
	client.close();
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
	NSC_DEBUG_MSG_STD("Command: " + cmd.first);
	NSC_DEBUG_MSG_STD("Arguments: " + cmd.second);

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
