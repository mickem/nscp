#include "StdAfx.h"
#include ".\nscommands.h"
#include <strEx.h>
#include <tlhelp32.h>



typedef std::hash_map<std::string,DWORD> NSPROCLST;

/**
 * Get a hash_map with all running processes.
 * @return a hash_map with all running processes
 */
NSPROCLST GetProcessList(void)
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	NSPROCLST ret;

	// Take a snapshot of all processes in the system.
	hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( hProcessSnap == INVALID_HANDLE_VALUE )
		throw "CreateToolhelp32Snapshot (of processes) failed";

	// Set the size of the structure before using it.
	pe32.dwSize = sizeof( PROCESSENTRY32 );

	// Retrieve information about the first process,
	// and exit if unsuccessful
	if( !Process32First( hProcessSnap, &pe32 ) ) {
		CloseHandle( hProcessSnap );     // Must clean up the snapshot object!
		throw "Process32First failed!";
	}

	// Now walk the snapshot of processes, and
	// display information about each process in turn
	do {
		ret[pe32.szExeFile] = pe32.th32ProcessID;
	} while( Process32Next( hProcessSnap, &pe32 ) );

	// Don't forget to clean up the snapshot object!
	CloseHandle( hProcessSnap );
	return ret;
}

/**
 * Retrieve the process state of one or more processes (by name).
 * Parse a list with a process names and verify that all names processes are running.
 * <pre>
 * Syntax:
 * request: 5&<showAll|showFail>&<service1>&<service2>&...
 * Return: <return state>&<service1> : <state1> - <service2> : <state2> - ...
 * <return state>	0 - All services OK
 *					1 - Unknown
 *					2 - At least one service is stopped
 *</pre>
 * @param args 
 * @return 
 * @todo Is this correct ? (has never been used so is most likely broken)
 */
NSClientCompat::returnBundle NSCommands::procState(std::list<std::string> args) {
	if (args.empty())
		return NSClientCompat::returnBundle(NSCAPI::returnCRIT, "ERROR: Missing argument exception.");
	NSPROCLST procs;
	std::string ret;
	ServiceState state = ok;
	bool showAll = args.front() == "ShowAll";
	args.pop_front();
	
	try {
		procs = GetProcessList();
	} catch (char *c) {
		NSC_LOG_ERROR_STD("ERROR: " + c);
		return NSClientCompat::returnBundle(NSCAPI::returnCRIT, (std::string)"ERROR: " + c);
	}
	for (std::list<std::string>::iterator it = args.begin();it!=args.end();it++) {
		std::string exe = (*it);
		NSPROCLST::iterator proc = procs.find(exe);

		if (proc == procs.end()) {
			if (state == ok)
				state = error;
			if (!ret.empty()) ret += " - ";
			ret += exe + " : not running";
		} else if (showAll) {
			if (!ret.empty()) ret += " - ";
			ret += exe + " : Running";
		}
	}
	if (ret.empty())
		ret ="All processes are running.";
	return NSClientCompat::returnBundle(state, ret);
}


/**
 * Helper function for serviceState.
 * Checks if a service is running and prepares a string representing the information we want to retrive
 *
 * @param bShowAll Show all running processes
 * @param &nState State of the current process
 * @param &info Service info struct
 * @return A string with service the current state
 */
std::string NSCommands::cmdServiceStateCheckItem(bool bShowAll, int &nState, TNtServiceInfo &info) {
	if (info.m_dwCurrentState == SERVICE_RUNNING) {
		if (bShowAll)
			return info.m_strServiceName + " : Started";
	}
	else if (info.m_dwCurrentState == SERVICE_STOPPED) {
		nState = 2;
		return info.m_strServiceName + " : Stopped";
	} else {
		if (nState < 1)
			nState = 1;
		return info.m_strServiceName + " : Unknown("+strEx::itos(nState)+")";
	}
	return "";
}
/**
 * Retrieve the service state of one or more services (by name).
 * Parse a list with a service names and verify that all named services are running.
 * <pre>
 * Syntax:
 * request: 5&<showAll|showFail>&<service1|_all_>&<service2>&...
 * Return: <return state>&<service1> : <state1> - <service2> : <state2> - ...
 * <return state>	0 - All services OK
 *					1 - Unknown
 *					2 - At least one service is stopped
 *</pre>
 *
 * @param args 
 * @return 
 */
NSClientCompat::returnBundle NSCommands::serviceState(std::list<std::string> args) {
	if (args.empty())
		return NSClientCompat::returnBundle(NSCAPI::returnCRIT, "ERROR: Missing argument exception.");
	else {
		std::string ret;
		int nState = 0;
		bool bShowAll = args.front() == "ShowAll";
		args.pop_front();

		if ((args.empty())||(args.front() == "_all_")) {
			TNtServiceInfoList list;
			TNtServiceInfo::EnumServices(SERVICE_WIN32, SERVICE_ACTIVE|SERVICE_INACTIVE, &list);
			for (TNtServiceInfoList::iterator it = list.begin(); it != list.end(); it ++) {
				std::string s = cmdServiceStateCheckItem(bShowAll, nState, *it);
				if (!s.empty() && !ret.empty())
					ret += " - ";
				ret += s;
			}
		} else {
			for (std::list<std::string>::iterator it = args.begin();it!=args.end();it++) {
				TNtServiceInfo info = TNtServiceInfo::GetService((*it).c_str());
				std::string s = cmdServiceStateCheckItem(bShowAll, nState, info);
				if (!s.empty() && !ret.empty())
					ret += " - ";
				ret += s;
			}
		}
		if (ret.empty())
			ret ="All services are running.";
		return NSClientCompat::returnBundle(nState, ret);
	}
}
/**
 * Check the used space of one ore more drives.
 * <pre>
 * Syntax:
 * request: <passwd>&4&<drive 1>&<drive 2>&...
 * Return: <free space 1>&<total space 1>&<free space 2>&<total space 2>&...
 *</pre>
 *
 * @param args A list of drives
 * @return A string with a list of drive usage stats
 */
NSClientCompat::returnBundle NSCommands::usedDiskSpace(std::list<std::string> args) {
	if (args.empty())
		return NSClientCompat::returnBundle(NSCAPI::returnCRIT, "ERROR: Missing argument exception.");
	std::string ret;
	for (std::list<std::string>::iterator it = args.begin();it!=args.end();it++) {
		std::string path = (*it);
		if (path.length() == 1)
			path += ":";
		if (GetDriveType(path.c_str()) != DRIVE_FIXED)
			return NSClientCompat::returnBundle(NSCAPI::returnCRIT, "ERROR: Drive is not a fixed drive." + path);
		ULARGE_INTEGER freeBytesAvailableToCaller;
		ULARGE_INTEGER totalNumberOfBytes;
		ULARGE_INTEGER totalNumberOfFreeBytes;
		if (!GetDiskFreeSpaceEx(path.c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes))
			return NSClientCompat::returnBundle(NSCAPI::returnCRIT, "ERROR: Could not get free space for" + path);
		ret += strEx::itos(static_cast<__int64>(totalNumberOfFreeBytes.QuadPart)) + "&";
		ret += strEx::itos(static_cast<__int64>(totalNumberOfBytes.QuadPart)) + "&";
	}
	return NSClientCompat::returnBundle(NSCAPI::returnUNKNOWN, ret);
}
