// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "NRPEListener.h"
#include <strEx.h>
#include <time.h>

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

#define DEFAULT_NRPE_PORT 5666


bool NRPEListener::loadModule() {
	timeout = NSCModuleHelper::getSettingsInt("NRPE", "commandTimeout", 60);
	std::list<std::string> commands = NSCModuleHelper::getSettingsSection("NRPE Handlers");
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

	simpleSocket::Socket::WSAStartup();
	socket.StartListen(NSCModuleHelper::getSettingsInt("NRPE", "port", DEFAULT_NRPE_PORT));
	return true;
}
bool NRPEListener::unloadModule() {
	socket.close();
	simpleSocket::Socket::WSACleanup();
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
	if (NSCModuleHelper::getSettingsInt("NRPE", "AllowArguments", 0) == 0) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit = arr.begin();
		int i=0;

		for (;cit!=arr.end();it++,i++) {
			strEx::replace(str, "ARG" + strEx::itos(i), (*cit));
		}
	}

	if (NSCModuleHelper::getSettingsInt("NRPE", "AllowNastyMetaChars", 0) == 0) {
		if (str.find_first_of(NASTY_METACHARS) != std::string::npos) {
			NSC_LOG_ERROR("Request command contained illegal metachars!");
			return NSCAPI::returnIgnored;
		}
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


NSC_WRAPPERS_MAIN_DEF(gNRPEListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEListener);
