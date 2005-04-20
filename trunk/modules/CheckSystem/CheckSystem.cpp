// NSClientCompat.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "CheckSystem.h"
#include <utils.h>
#include <tlhelp32.h>
#include <EnumNtSrv.h>

CheckSystem gNSClientCompat;

/**
 * DLL Entry point
 * @param hModule 
 * @param ul_reason_for_call 
 * @param lpReserved 
 * @return 
 */
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

/**
 * Default c-tor
 * @return 
 */
CheckSystem::CheckSystem() {}
/**
 * Default d-tor
 * @return 
 */
CheckSystem::~CheckSystem() {}
/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckSystem::loadModule() {
	pdhThread.createThread();
	return true;
}
/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckSystem::unloadModule() {
	if (!pdhThread.exitThread(20000))
		NSC_LOG_ERROR("Could not exit the thread, memory leak and potential corruption may be the result...");
	return true;
}
/**
 * Return the module name.
 * @return The module name
 */
std::string CheckSystem::getModuleName() {
	return "System Checks Module.";
}
/**
 * Module version
 * @return module version
 */
NSCModuleWrapper::module_version CheckSystem::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
}
/**
 * Check if we have a command handler.
 * @return true (as we have a command handler)
 */
bool CheckSystem::hasCommandHandler() {
	return true;
}
/**
 * Check if we have a message handler.
 * @return false as we have no message handler
 */
bool CheckSystem::hasMessageHandler() {
	return false;
}
/*
*/
/**
 * Main command parser and delegator.
 * This also handles a lot of the simpler responses (though some are deferred to other helper functions)
 *
 *
 * @param command 
 * @param argLen 
 * @param **args 
 * @return 
 */
NSCAPI::nagiosReturn CheckSystem::handleCommand(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) {
	std::list<std::string> stl_args;
	CheckSystem::returnBundle rb;
	if (command == "checkCPU") {
		return checkCPU(command, argLen, char_args, msg, perf);
	} else if (command == "checkUpTime") {
		return checkUpTime(command, argLen, char_args, msg, perf);
	} else if (command == "checkServiceState") {
		return checkServiceState(command, argLen, char_args, msg, perf);
	} else if (command == "checkProcState") {
		return checkProcState(command, argLen, char_args, msg, perf);
	} else if (command == "checkMem") {
		return checkMem(command, argLen, char_args, msg, perf);
	} else if (command == "checkCounter") {
		return checkCounter(command, argLen, char_args, msg, perf);
	}
/*
		case REQ_PROCSTATE:
			rb = NSCommands::procState(arrayBuffer::arrayBuffer2list(argLen, char_args));
			msg = rb.msg_;
			perf = rb.perf_;
			return rb.code_;
	*/
	return NSCAPI::returnIgnored;
}

// checkCPU warn=80 crit=90 time=20m time=10s time=4
// checkCPU warn=80 crit=90 time=20m time=10s time=4 showAll
// checkCPU 20 10 4 nsclient
NSCAPI::nagiosReturn CheckSystem::checkCPU(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) 
{
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	int warn;
	int crit;
	std::list<std::string> times;
	bool bNSCLientCompatible = false;
	bool bShowAll = false;
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;


	for (arrayBuffer::arrayList::const_iterator it = stl_args.begin(); it != stl_args.end(); ++it) {
		strEx::token t = strEx::getToken((*it), '=');
		if (t.first == "crit")
			crit = strEx::stoi(t.second);
		else if (t.first == "warn")
			warn = strEx::stoi(t.second);
		else if (t.first == "time")
			times.push_back(t.second);
		else if (t.first == NSCLIENT)
			bNSCLientCompatible = true;
		else if (t.first == "showAll")
			bShowAll = true;
		else
			times.push_back(t.first);
	}

	for (std::list<std::string>::iterator it = times.begin(); it != times.end(); ++it) {
		PDHCollector *pObject = pdhThread.getThread();
		assert(pObject);
		if (bNSCLientCompatible) {
			int v = pObject->getCPUAvrage((*it) + "m");
			if (v == -1) {
				msg = "ERROR: We don't collect data this far back: " + (*it);
				return NSCAPI::returnUNKNOWN;
			}
			if (!msg.empty()) msg += "&";
			msg += strEx::itos(v);
		} else {
			int v = pObject->getCPUAvrage((*it));
			if (v == -1) {
				msg = "ERROR: We don't collect data this far back: " + (*it);
				return NSCAPI::returnUNKNOWN;
			} else {
				if (v > warn) {
					NSCHelper::escalteReturnCodeToWARN(ret);
					msg += strEx::itos(v) + "% > " + strEx::itos(warn) + " " + (*it);
				} if (v > crit) {
					NSCHelper::escalteReturnCodeToCRIT(ret);
					msg += strEx::itos(v) + "% > " + strEx::itos(crit) + " " + (*it);
				} else if (bShowAll) {
					msg += strEx::itos(v) + "% ";
				}
				perf += "'" + (*it) + " average'=" + strEx::itos(v) + "%;" + strEx::itos(warn) + ";" + strEx::itos(crit) + "; ";
			}
		}
	}
	if (bNSCLientCompatible) {
		// Don't prefix/postfix the output for NSClient
	} else if (msg.empty()) {
		msg = "CPU Load ok.";
	} else {
		msg = "CPU Load: " + msg;
	}
	return ret;
}

// checkUpTime crit=1d warn=6h
// checkUpTime nsclient
NSCAPI::nagiosReturn CheckSystem::checkUpTime(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	unsigned long long warn;
	unsigned long long crit;
	bool bNSCLientCompatible = false;

	for (arrayBuffer::arrayList::const_iterator it = stl_args.begin(); it != stl_args.end(); ++it) {
		strEx::token t = strEx::getToken((*it), '=');
		if (t.first == "crit")
			crit = strEx::stoi64_as_time(t.second);
		else if (t.first == "warn")
			warn = strEx::stoi64_as_time(t.second);
		else if (t.first == NSCLIENT)
			bNSCLientCompatible = true;
		else {
			msg = "Invalid argument: " + t.first;
			return NSCAPI::returnUNKNOWN;
		}
	}
	PDHCollector *pObject = pdhThread.getThread();
	assert(pObject);
	long long uptime = pObject->getUptime();
	if (bNSCLientCompatible) {
		msg = strEx::itos(uptime);
		return NSCAPI::returnOK;
	} else {
		uptime *= 1000;
		if (uptime < crit) {
			msg = "Client has uptime (" + strEx::itos_as_time(uptime) + ") < critical (" + strEx::itos_as_time(crit) + ")";
			return NSCAPI::returnCRIT;
		}
		if (uptime < warn) {
			msg = "Client has uptime (" + strEx::itos_as_time(uptime) + ") < warning (" + strEx::itos_as_time(warn) + ")";
			return NSCAPI::returnWARN;
		}
	}
	return NSCAPI::returnOK;
}




/**
 * Retrieve the service state of one or more services (by name).
 * Parse a list with a service names and verify that all named services are running.
 * <pre>
 * Syntax:
 * request: checkServiceState <option> [<option> [...]]
 * Return: <return state>&<service1> : <state1> - <service2> : <state2> - ...
 * Available options:
 *		<name>=<state>	Check if a service has a specific state
 *			State can be wither started or stopped
 *		ShowAll			Show the state of all listed service. If not set only critical services are listed.
 * Examples:
 * checkServiceState showAll myService MyService
 *</pre>
 *
 * @param args 
 * @return 
 */
NSCAPI::nagiosReturn CheckSystem::checkServiceState(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<std::pair<std::string,states> > services;
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	bool bShowAll = false;

	for (arrayBuffer::arrayList::const_iterator it = stl_args.begin(); it != stl_args.end(); ++it) {
		strEx::token t = strEx::getToken((*it), '=');
		if (t.first == SHOW_ALL)
			bShowAll = true;
		else if (t.first == SHOW_FAIL)  {
			bShowAll = false;
		} else {
			if (t.second.empty())
				services.push_back(std::pair<std::string,states>(t.first, started));
			else {
				if (t.second == "started")
					services.push_back(std::pair<std::string,states>(t.first, started));
				else
					services.push_back(std::pair<std::string,states>(t.first, stopped));
			}
		}
	}
	for (std::list<std::pair<std::string,states> >::iterator it = services.begin(); it != services.end(); ++it) {
		TNtServiceInfo info = TNtServiceInfo::GetService((*it).first.c_str());
		std::string tmp;
		if ( (info.m_dwCurrentState == SERVICE_RUNNING) && ((*it).second == started) ) {
			if (bShowAll)
				tmp = info.m_strServiceName + " : Started";
		} else if ( (info.m_dwCurrentState == SERVICE_STOPPED) && ((*it).second == stopped) ) {
			if (bShowAll)
				tmp = info.m_strServiceName + " : Stopped";
		} else if ((info.m_dwCurrentState == SERVICE_STOPPED) && ((*it).second == started) ) {
			NSCHelper::escalteReturnCodeToCRIT(ret);
			tmp = info.m_strServiceName + " : Stopped";
		} else if ((info.m_dwCurrentState == SERVICE_RUNNING) && ((*it).second == stopped) ) {
			NSCHelper::escalteReturnCodeToCRIT(ret);
			tmp = info.m_strServiceName + " : Started";
		} else {
			NSCHelper::escalteReturnCodeToWARN(ret);
			tmp = info.m_strServiceName + " : Unknown";
		}
		if (!msg.empty()&&!tmp.empty())
			msg += " - ";
		msg += tmp;
	}
	if (msg.empty())
		msg ="All services ok.";
	return ret;
}


// checkMem showAll maxWarn=50 maxCrit=75
NSCAPI::nagiosReturn CheckSystem::checkMem(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<std::pair<std::string,states> > services;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bNSCLientCompatible = false;

	checkHolders::SizeMaxMin warn;
	checkHolders::SizeMaxMin crit;

	for (arrayBuffer::arrayList::const_iterator it = stl_args.begin(); it != stl_args.end(); ++it) {
		strEx::token t = strEx::getToken((*it), '=');
		if (t.first == SHOW_ALL)
			bShowAll = true;
		else if (t.first == "MaxWarn") {
			warn.max.set(t.second);
		} else if (t.first == "MinWarn") {
			warn.min.set(t.second);
		} else if (t.first == "MaxCrit") {
			crit.max.set(t.second);
		} else if (t.first == "MinCrit") {
			crit.min.set(t.second);
		} else if (t.first == NSCLIENT)
			bNSCLientCompatible = true;
		else {
			msg = "Invalid argument: " + t.first;
			return NSCAPI::returnUNKNOWN;
		}
	}

	PDHCollector *pObject = pdhThread.getThread();
	assert(pObject);
	long long pageCommit = pObject->getMemCommit(); 
	long long pageCommitLimit = pObject->getMemCommitLimit(); 
	if (bNSCLientCompatible) {
		msg = strEx::itos(pageCommitLimit) + "&" + strEx::itos(pageCommit);
		return NSCAPI::returnOK;
	} else {
		std::string tStr;
		if (crit.max.hasBounds() && crit.max.checkMAX(pageCommit, pageCommitLimit)) {
			tStr = crit.max.prettyPrint("page", pageCommit, pageCommitLimit) + " > critical";
			NSCHelper::escalteReturnCodeToCRIT(returnCode);
		} else if (crit.min.hasBounds() && crit.min.checkMIN(pageCommit, pageCommitLimit)) {
			tStr = crit.min.prettyPrint("page", pageCommit, pageCommitLimit) + " < critical";
			NSCHelper::escalteReturnCodeToCRIT(returnCode);
		} else if (warn.max.hasBounds() && warn.max.checkMAX(pageCommit, pageCommitLimit)) {
			tStr = warn.max.prettyPrint("page", pageCommit, pageCommitLimit) + " > warning";
			NSCHelper::escalteReturnCodeToWARN(returnCode);
		} else if (warn.min.hasBounds() && warn.min.checkMIN(pageCommit, pageCommitLimit)) {
			tStr = warn.min.prettyPrint("page", pageCommit, pageCommitLimit) + " < warning";
			NSCHelper::escalteReturnCodeToWARN(returnCode);
		} else if (bShowAll) {
			tStr = "page: " + strEx::itos_as_BKMG(pageCommit);
		}
		perf += checkHolders::SizeMaxMin::printPerf("page", pageCommit, pageCommitLimit, warn, crit);
		msg += tStr;
	}
	if (msg.empty())
		msg ="Memory ok.";
	return returnCode;
}

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

NSCAPI::nagiosReturn CheckSystem::checkProcState(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<std::pair<std::string,states> > procs;
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	bool bShowAll = false;

	for (arrayBuffer::arrayList::const_iterator it = stl_args.begin(); it != stl_args.end(); ++it) {
		strEx::token t = strEx::getToken((*it), '=');
		if (t.first == SHOW_ALL)
			bShowAll = true;
		else if (t.first == SHOW_FAIL)  {
			bShowAll = false;
		} else {
			if (t.second.empty())
				procs.push_back(std::pair<std::string,states>(t.first, started));
			else {
				if (t.second == "started")
					procs.push_back(std::pair<std::string,states>(t.first, started));
				else
					procs.push_back(std::pair<std::string,states>(t.first, stopped));
			}
		}
	}
	NSPROCLST runningProcs;
	try {
		runningProcs = GetProcessList();
	} catch (char *c) {
		NSC_LOG_ERROR_STD("ERROR: " + c);
		msg = static_cast<std::string>("ERROR: ") + c;
		return NSCAPI::returnCRIT;
	}

	for (std::list<std::pair<std::string,states> >::iterator it = procs.begin(); it != procs.end(); ++it) {
		NSPROCLST::iterator proc = runningProcs.find((*it).first);
		bool bFound = proc != runningProcs.end();
		std::string tmp;
		if ( (bFound) && ((*it).second == started) ) {
			if (bShowAll)
				tmp = (*it).first + " : Running";
		} else if ( (!bFound) && ((*it).second == stopped) ) {
			if (bShowAll)
				tmp = (*it).first + " : Stopped";
		} else if ( (!bFound) && ((*it).second == started) ) {
			NSCHelper::escalteReturnCodeToCRIT(ret);
			tmp = (*it).first + " : Stopped";
		} else if ( (bFound) && ((*it).second == stopped) ) {
			NSCHelper::escalteReturnCodeToCRIT(ret);
			tmp = (*it).first + " : Running";
		}
		if (!msg.empty()&&!tmp.empty())
			msg += " - ";
		msg += tmp;
	}
	if (msg.empty())
		msg ="All processes ok.";
	return ret;
}

NSCAPI::nagiosReturn CheckSystem::checkCounter(const std::string command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<std::string> counters;
	NSCAPI::nagiosReturn ret = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bNSCLientCompatible = false;

	for (arrayBuffer::arrayList::const_iterator it = stl_args.begin(); it != stl_args.end(); ++it) {
		strEx::token t = strEx::getToken((*it), '=');
		if (t.first == SHOW_ALL)
			bShowAll = true;
		else if (t.first == SHOW_FAIL)  {
			bShowAll = false;
		} else if (t.first == NSCLIENT) {
			bNSCLientCompatible = true;
		} else if (t.first == "counter") {
			counters.push_back(t.second);
		} else {
			counters.push_back(t.first);
		}
	}

	for (std::list<std::string>::iterator it = counters.begin(); it != counters.end(); ++it) {
		try {
			try {
				PDH::PDHQuery pdh;
				PDHCollectors::StaticPDHCounterListener counter;
				pdh.addCounter((*it), &counter);
				pdh.open();
				pdh.collect();
				msg += strEx::itos(counter.getValue());
				pdh.close();
			} catch (const PDH::PDHException &e) {
				NSC_LOG_ERROR_STD("ERROR: " + e.str_);
				msg = static_cast<std::string>("ERROR: ") + e.str_;
				return 0;
			}
		} catch (PDH::PDHException e) {
			NSC_LOG_ERROR_STD("ERROR: " + e.str_);
			msg = static_cast<std::string>("ERROR: ") + e.str_;
			return NSCAPI::returnCRIT;
		}
	}
	if (msg.empty())
		msg ="uhmm...";
	return ret;
}
NSC_WRAPPERS_MAIN_DEF(gNSClientCompat);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNSClientCompat);
