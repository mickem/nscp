// NSClientCompat.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "CheckSystem.h"
#include <utils.h>
#include <tlhelp32.h>
#include <EnumNtSrv.h>
#include <EnumProcess.h>
#include <sysinfo.h>
#include <checkHelpers.hpp>

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
CheckSystem::CheckSystem() : processMethod_(0) {}
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

	std::string wantedMethod = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_ENUMPROC_METHOD, C_SYSTEM_ENUMPROC_METHOD_DEFAULT);

	CEnumProcess tmp;
	int method = tmp.GetAvailableMethods();



	if (wantedMethod == C_SYSTEM_ENUMPROC_METHOD_AUTO) {
		OSVERSIONINFO osVer = systemInfo::getOSVersion();
		if (systemInfo::isBelowNT4(osVer)) {
			NSC_DEBUG_MSG_STD("Autodetected NT4<, using PSAPI process enumeration.");
			processMethod_ = ENUM_METHOD::PSAPI;
		} else if (systemInfo::isAboveW2K(osVer)) {
			NSC_DEBUG_MSG_STD("Autodetected W2K>, using TOOLHELP process enumeration.");
			processMethod_ = ENUM_METHOD::TOOLHELP;
		} else {
			NSC_DEBUG_MSG_STD("Autodetected failed, using PSAPI process enumeration.");
			processMethod_ = ENUM_METHOD::PSAPI;
		}
	} else if (wantedMethod == C_SYSTEM_ENUMPROC_METHOD_PSAPI) {
		NSC_DEBUG_MSG_STD("Using PSAPI method.");
		if (method == (method|ENUM_METHOD::PSAPI)) {
			processMethod_ = ENUM_METHOD::PSAPI;
		} else {
			NSC_LOG_ERROR_STD("PSAPI method not available, check " C_SYSTEM_ENUMPROC_METHOD " option.");
		}
	} else {
		NSC_DEBUG_MSG_STD("Using TOOLHELP method.");
		if (method == (method|ENUM_METHOD::TOOLHELP)) {
			processMethod_ = ENUM_METHOD::TOOLHELP;
		} else {
			NSC_LOG_ERROR_STD("TOOLHELP method not avalible, check " C_SYSTEM_ENUMPROC_METHOD " option.");
		}
	}
	return true;
}
/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckSystem::unloadModule() {
	if (!pdhThread.exitThread(20000)) {
		std::cout << "MAJOR ERROR: Could not unload thread..." << std::endl;
		NSC_LOG_ERROR("Could not exit the thread, memory leak and potential corruption may be the result...");
	}
	return true;
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
NSCAPI::nagiosReturn CheckSystem::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) {
	std::list<std::string> stl_args;
	CheckSystem::returnBundle rb;
	if (command == "checkCPU") {
		return checkCPU(argLen, char_args, msg, perf);
	} else if (command == "checkUpTime") {
		return checkUpTime(argLen, char_args, msg, perf);
	} else if (command == "checkServiceState") {
		return checkServiceState(argLen, char_args, msg, perf);
	} else if (command == "checkProcState") {
		return checkProcState(argLen, char_args, msg, perf);
	} else if (command == "checkMem") {
		return checkMem(argLen, char_args, msg, perf);
	} else if (command == "checkCounter") {
		return checkCounter(argLen, char_args, msg, perf);
	}
	return NSCAPI::returnIgnored;
}

NSCAPI::nagiosReturn CheckSystem::checkCPU(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) 
{
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsInteger> CPULoadConatiner;

	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CPULoadConatiner> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bNSClient = false;
	CPULoadConatiner tmpObject;

	tmpObject.data = "cpuload";

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_STR("MaxWarn", tmpObject.warn.max)
		MAP_OPTIONS_STR("warn", tmpObject.warn.max)
		MAP_OPTIONS_STR("MinWarn", tmpObject.warn.min)
		MAP_OPTIONS_STR("MaxCrit", tmpObject.crit.max)
		MAP_OPTIONS_STR("crit", tmpObject.crit.max)
		MAP_OPTIONS_STR("MinCrit", tmpObject.crit.min)
		MAP_OPTIONS_STR_AND("time", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND("Time", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_BOOL_TRUE(SHOW_ALL, bShowAll)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
			MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			else if (p2.first == "Time") {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				list.push_back(tmpObject);
			}
			MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
			MAP_OPTIONS_SECONDARY_END()
		else { tmpObject.data = p__.first, list.push_back(tmpObject); }
	MAP_OPTIONS_END()

	for (std::list<CPULoadConatiner>::const_iterator it = list.begin(); it != list.end(); ++it) {
		CPULoadConatiner load = (*it);
		PDHCollector *pObject = pdhThread.getThread();
		if (!pObject) {
			msg = "ERROR: PDH Collection thread not running.";
			return NSCAPI::returnUNKNOWN;
		}
		if (bNSClient) {
			int value = pObject->getCPUAvrage(load.data + "m");
			if (value == -1) {
				msg = "ERROR: We don't collect data this far back: " + load.getAlias();
				return NSCAPI::returnUNKNOWN;
			}
			if (!msg.empty()) msg += "&";
			msg += strEx::itos(value);
		} else {
			int value = pObject->getCPUAvrage(load.data);
			if (value == -1) {
				msg = "ERROR: We don't collect data this far back: " + load.getAlias();
				return NSCAPI::returnUNKNOWN;
			} else {
				load.setDefault(tmpObject);
				load.runCheck(value, returnCode, msg, perf, bShowAll);
			}
		}
	}

	if (msg.empty())
		msg = "OK CPU Load ok.";
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}

NSCAPI::nagiosReturn CheckSystem::checkUpTime(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsTime> UpTimeConatiner;

	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bNSClient = false;
	UpTimeConatiner bounds;

	bounds.data = "uptime";

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_STR("MaxWarn", bounds.warn.max)
		MAP_OPTIONS_STR("warn", bounds.warn.min)
		MAP_OPTIONS_STR("MinWarn", bounds.warn.min)
		MAP_OPTIONS_STR("MaxCrit", bounds.crit.max)
		MAP_OPTIONS_STR("crit", bounds.crit.min)
		MAP_OPTIONS_STR("MinCrit", bounds.crit.min)
		MAP_OPTIONS_STR("Alias", bounds.data)
		MAP_OPTIONS_BOOL_TRUE(SHOW_ALL, bShowAll)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_MISSING(msg, "Unknown argument: ")
	MAP_OPTIONS_END()


	PDHCollector *pObject = pdhThread.getThread();
	if (!pObject) {
		msg = "ERROR: PDH Collection thread not running.";
		return NSCAPI::returnUNKNOWN;
	}
	unsigned long long value = pObject->getUptime();
	if (bNSClient) {
		msg = strEx::itos(value);
	} else {
		value *= 1000;
		bounds.runCheck(value, returnCode, msg, perf, bShowAll);
	}

	if (msg.empty())
		msg = "OK all counters within bounds.";
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}

// @todo state_handler



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
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in 
 * @return The status of the command
 */
NSCAPI::nagiosReturn CheckSystem::checkServiceState(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckConatiner<checkHolders::SimpleBoundsStateBoundsInteger> StateConatiner;
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<StateConatiner> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bNSClient = false;
	StateConatiner tmpObject;

	tmpObject.data = "uptime";
	tmpObject.warn.state = "started";

	MAP_OPTIONS_BEGIN(stl_args)
		/*
		MAP_OPTIONS_STR("MaxWarn", tmpObject.warn.max)
		MAP_OPTIONS_STR("warn", tmpObject.warn.min)
		MAP_OPTIONS_STR("MinWarn", tmpObject.warn.min)
		MAP_OPTIONS_STR("MaxCrit", tmpObject.crit.max)
		MAP_OPTIONS_STR("crit", tmpObject.crit.min)
		MAP_OPTIONS_STR("MinCrit", tmpObject.crit.min)
		*/
		MAP_OPTIONS_STR("Alias", tmpObject.data)
		MAP_OPTIONS_BOOL_TRUE(SHOW_ALL, bShowAll)
		MAP_OPTIONS_BOOL_FALSE(SHOW_FAIL, bShowAll)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			else if (p2.first == "Time") {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				list.push_back(tmpObject);
			}
			MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		else { 
			tmpObject.data = p__.first;
			if (p__.second.empty())
				tmpObject.crit.state = "started"; 
			else
				tmpObject.crit.state = p__.second; 
			list.push_back(tmpObject); 
		}
	MAP_OPTIONS_END()

	for (std::list<StateConatiner>::iterator it = list.begin(); it != list.end(); ++it) {
		TNtServiceInfo info;
		if (bNSClient) {
			try {
				info = TNtServiceInfo::GetService((*it).data.c_str());
			} catch (NTServiceException e) {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": Unknown";
				NSCHelper::escalteReturnCodeToWARN(returnCode);
				continue;
			}
			if ((info.m_dwCurrentState == SERVICE_RUNNING) && (bShowAll)) {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": Started";
			} else if (info.m_dwCurrentState == SERVICE_RUNNING) {
			} else if (info.m_dwCurrentState == SERVICE_STOPPED) {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": Stopped";
				NSCHelper::escalteReturnCodeToCRIT(returnCode);
			} else {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": Unknown";
				NSCHelper::escalteReturnCodeToWARN(returnCode);
			}
		} else {
			try {
				info = TNtServiceInfo::GetService((*it).data.c_str());
			} catch (NTServiceException e) {
				NSC_LOG_ERROR_STD(e.getError());
				msg = e.getError();
				return NSCAPI::returnUNKNOWN;
			}
			checkHolders::state_type value;
			if (info.m_dwCurrentState == SERVICE_RUNNING)
				value = checkHolders::state_started;
			else if (info.m_dwCurrentState == SERVICE_STOPPED)
				value = checkHolders::state_stopped;
			else
				value = checkHolders::state_none;
			(*it).runCheck(value, returnCode, msg, perf, bShowAll);
		}

	}
	if (msg.empty())
		msg = "OK: All services are running.";
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}


/**
 * Check available memory and return various check results
 * Example: checkMem showAll maxWarn=50 maxCrit=75
 *
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in 
 * @return The status of the command
 */
NSCAPI::nagiosReturn CheckSystem::checkMem(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinPercentageBoundsInt64 > MemoryConatiner;
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bNSClient = false;
	MemoryConatiner bounds;

	bounds.data = "page";

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_STR("MaxWarn", bounds.warn.max)
		MAP_OPTIONS_STR("MinWarn", bounds.warn.min)
		MAP_OPTIONS_STR("MaxCrit", bounds.crit.max)
		MAP_OPTIONS_STR("MinCrit", bounds.crit.min)
		MAP_OPTIONS_STR("Alias", bounds.data)
		MAP_OPTIONS_BOOL_TRUE(SHOW_ALL, bShowAll)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_MISSING(msg, "Unknown argument: ")
	MAP_OPTIONS_END()

	PDHCollector *pObject = pdhThread.getThread();
	if (!pObject) {
		msg = "ERROR: PDH Collection thread not running.";
		return NSCAPI::returnUNKNOWN;
	}
	long long pageCommit = pObject->getMemCommit(); 
	long long pageCommitLimit = pObject->getMemCommitLimit(); 
	if (bNSClient) {
		msg = strEx::itos(pageCommitLimit) + "&" + strEx::itos(pageCommit);
		return NSCAPI::returnOK;
	} else {
		bounds.warn.max.setMax(pageCommitLimit);
		bounds.warn.min.setMax(pageCommitLimit);
		bounds.crit.max.setMax(pageCommitLimit);
		bounds.crit.min.setMax(pageCommitLimit);
		bounds.runCheck(pageCommit, returnCode, msg, perf, bShowAll);
	}
	if (msg.empty())
		msg = "OK memory within bounds.";
	else
		msg = NSCHelper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}
typedef struct NSPROCDATA__ {
	NSPROCDATA__() : count(0) {}
	NSPROCDATA__(const NSPROCDATA__ &other) {
		count = other.count;
		entry = other.entry;
	}

	unsigned int count;
	CEnumProcess::CProcessEntry entry;
} NSPROCDATA;
typedef std::hash_map<std::string,NSPROCDATA> NSPROCLST;
/**
* Get a hash_map with all running processes.
* @return a hash_map with all running processes
*/
NSPROCLST GetProcessList(int processMethod)
{
	NSPROCLST ret;
	if (processMethod == 0) {
		NSC_LOG_ERROR_STD("ProcessMethod not defined or not available.");
		return ret;
	}
	CEnumProcess enumeration;
	enumeration.SetMethod(processMethod);
	CEnumProcess::CProcessEntry entry;
	for (BOOL OK = enumeration.GetProcessFirst(&entry); OK; OK = enumeration.GetProcessNext(&entry) ) {
		NSPROCLST::iterator it = ret.find(entry.sFilename);
		if (it == ret.end())
			ret[entry.sFilename].entry = entry;
		else
			(*it).second.count++;
	}
	return ret;
}

/**
 * Check process state and return result
 *
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in 
 * @return The status of the command
 */
NSCAPI::nagiosReturn CheckSystem::checkProcState(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinStateBoundsStateBoundsInteger> StateConatiner;
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<StateConatiner> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bNSClient = false;
	StateConatiner tmpObject;

	tmpObject.data = "uptime";
	tmpObject.crit.state = "started";

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_STR("MaxWarnCount", tmpObject.warn.max)
		MAP_OPTIONS_STR("MinWarnCount", tmpObject.warn.min)
		MAP_OPTIONS_STR("MaxCritCount", tmpObject.crit.max)
		MAP_OPTIONS_STR("MinCritCount", tmpObject.crit.min)
		MAP_OPTIONS_STR("Alias", tmpObject.alias)
		MAP_OPTIONS_BOOL_TRUE(SHOW_ALL, bShowAll)
		MAP_OPTIONS_BOOL_FALSE(SHOW_FAIL, bShowAll)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
		else if (p2.first == "Proc") {
			tmpObject.data = p__.second;
			tmpObject.alias = p2.second;
			list.push_back(tmpObject);
		}
		MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
			MAP_OPTIONS_SECONDARY_END()
		else { 
			tmpObject.data = p__.first;
			if (p__.second.empty())
				tmpObject.crit.state = "started"; 
			else
				tmpObject.crit.state = p__.second; 
			list.push_back(tmpObject); 
		}
	MAP_OPTIONS_END()


	NSPROCLST runningProcs;
	try {
		runningProcs = GetProcessList(processMethod_);
	} catch (char *c) {
		NSC_LOG_ERROR_STD("ERROR: " + c);
		msg = static_cast<std::string>("ERROR: ") + c;
		return NSCAPI::returnUNKNOWN;
	}

	for (std::list<StateConatiner>::iterator it = list.begin(); it != list.end(); ++it) {
		NSPROCLST::iterator proc = runningProcs.find((*it).data);
		bool bFound = proc != runningProcs.end();
		std::string tmp;
		TNtServiceInfo info;
		if (bNSClient) {
			if (bFound && bShowAll) {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": Started";
			} else if (bFound) {
			} else {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": Stopped";
				NSCHelper::escalteReturnCodeToCRIT(returnCode);
			}
		} else {
			checkHolders::MaxMinStateValueType<int, checkHolders::state_type> value;
			if (bFound) {
				value.count = (*proc).second.count;
				value.state = checkHolders::state_started;
			} else {
				value.count = 0;
				value.state = checkHolders::state_stopped;
			}
			(*it).runCheck(value, returnCode, msg, perf, bShowAll);
		}

	}
	if (msg.empty())
		msg = "OK: All processes are running.";
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}


/**
 * Check a counter and return the value
 *
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in 
 * @return The status of the command
 *
 * @todo add parsing support for NRPE
 */
NSCAPI::nagiosReturn CheckSystem::checkCounter(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf)
{
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsDouble> CounterConatiner;

	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CounterConatiner> counters;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bNSClient = false;
	/* average maax */
	bool bCheckAverages = true; 
	unsigned int averageDelay = 1000;
	CounterConatiner tmpObject;

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_STR_AND("Counter", tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_STR("MaxWarn", tmpObject.warn.max)
		MAP_OPTIONS_STR("MinWarn", tmpObject.warn.min)
		MAP_OPTIONS_STR("MaxCrit", tmpObject.crit.max)
		MAP_OPTIONS_STR("MinCrit", tmpObject.crit.min)
		MAP_OPTIONS_STR("Alias", tmpObject.data)
		MAP_OPTIONS_BOOL_TRUE(SHOW_ALL, bShowAll)
		MAP_OPTIONS_BOOL_FALSE(SHOW_FAIL, bShowAll)
		MAP_OPTIONS_BOOL_EX("Averages", bCheckAverages, "true", "false")
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			else if (p2.first == "Counter") {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				counters.push_back(tmpObject);
			}
			MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		else {
			tmpObject.data = p__.first;
			counters.push_back(tmpObject);
		}
	MAP_OPTIONS_END()

	for (std::list<CounterConatiner>::const_iterator cit = counters.begin(); cit != counters.end(); ++cit) {
		CounterConatiner counter = (*cit);
		try {
			std::string tstr;
			if (!PDH::Enumerations::validate(counter.data, tstr)) {
				msg = tstr;
				return NSCAPI::returnUNKNOWN;
			}
			PDH::PDHQuery pdh;
			PDHCollectors::StaticPDHCounterListener<double, PDH_FMT_DOUBLE> cDouble;
			pdh.addCounter(counter.data, &cDouble);
			pdh.open();
			pdh.collect();
			Sleep(1000);
			pdh.gatherData();
			pdh.close();
			double value = cDouble.getValue();
			std::cout << "Collected double data: " << value << std::endl;
			if (bNSClient) {
				msg += strEx::itos(value);
			} else {
				counter.setDefault(tmpObject);
				counter.runCheck(value, returnCode, msg, perf, bShowAll);
			}
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD("ERROR: " + e.getError() + " (" + counter.getAlias() + ")");
			msg = static_cast<std::string>("ERROR: ") + e.getError();
			return NSCAPI::returnUNKNOWN;
		}
	}

	if (msg.empty())
		msg = "OK all counters within bounds.";
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}
NSC_WRAPPERS_MAIN_DEF(gNSClientCompat);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNSClientCompat);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNSClientCompat);



MODULE_SETTINGS_START(CheckSystem, "System check module", "...")

PAGE("Check options")

ITEM_EDIT_TEXT("Check resolution", "This is how often the PDH data is polled and stored in the CPU buffer. (this is enterd in 1/th: of a second)")
OPTION("unit", "1/10:th of a second")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "Check System")
OPTION("key", "CheckResolution")
OPTION("default", "10")
ITEM_END()

ITEM_EDIT_TEXT("CPU buffer size", "This is the size of the buffer that stores CPU history.")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "Check System")
OPTION("key", "CPUBufferSize")
OPTION("default", "1h")
ITEM_END()

PAGE_END()
ADVANCED_PAGE("Compatiblity settings")

ITEM_EDIT_TEXT("MemoryCommitByte", "The memory commited bytes used to calculate the avalible memory.")
OPTION("disableCaption", "Attempt to autodetect this.")
OPTION("disabled", "auto")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "Check System")
OPTION("key", "MemoryCommitByte")
OPTION("default", "auto")
ITEM_END()

ITEM_EDIT_TEXT("MemoryCommitLimit", "The memory commit limit used to calculate the avalible memory.")
OPTION("disableCaption", "Attempt to autodetect this.")
OPTION("disabled", "auto")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "Check System")
OPTION("key", "MemoryCommitLimit")
OPTION("default", "auto")
ITEM_END()

ITEM_EDIT_TEXT("SystemSystemUpTime", "The PDH counter for the System uptime.")
OPTION("disableCaption", "Attempt to autodetect this.")
OPTION("disabled", "auto")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "Check System")
OPTION("key", "SystemSystemUpTime")
OPTION("default", "auto")
ITEM_END()

ITEM_EDIT_TEXT("SystemTotalProcessorTime", "The PDH conter usaed to measure CPU load.")
OPTION("disableCaption", "Attempt to autodetect this.")
OPTION("disabled", "auto")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "Check System")
OPTION("key", "SystemTotalProcessorTime")
OPTION("default", "auto")
ITEM_END()

ITEM_EDIT_TEXT("ProcessEnumerationMethod", "The method to use when enumerating processes")
OPTION("count", "3")
OPTION("caption_1", "Autodetect (TOOLHELP for NT/4 and PSAPI for W2k)")
OPTION("value_1", "auto")
OPTION("caption_2", "TOOLHELP use this for NT/4 systems")
OPTION("value_2", "TOOLHELP")
OPTION("caption_3", "PSAPI use this for W2k (and abowe) systems")
OPTION("value_3", "PSAPI")
ITEM_MAP_TO("basic_ini_text_mapper")
OPTION("section", "Check System")
OPTION("key", "ProcessEnumerationMethod")
OPTION("default", "auto")
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()