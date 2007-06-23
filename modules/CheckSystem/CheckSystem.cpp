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
#include "CheckSystem.h"
#include <utils.h>
#include <tlhelp32.h>
#include <EnumNtSrv.h>
#include <EnumProcess.h>
#include <checkHelpers.hpp>
#include <map>
#include <set>
#include <sysinfo.h>

CheckSystem gCheckSystem;

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
CheckSystem::CheckSystem() : processMethod_(0), pdhThread("pdhThread") {}
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

int CheckSystem::commandLineExec(const char* command,const unsigned int argLen,char** args) {
	if (_stricmp(command, "debugpdh") == 0) {
		PDH::Enumerations::Objects lst;
		try {
			lst = PDH::Enumerations::EnumObjects();
		} catch (const PDH::PDHException e) {
			std::cout << "Service enumeration failed: " << e.getError();
			return 0;
		}
		for (PDH::Enumerations::Objects::iterator it = lst.begin();it!=lst.end();++it) {
			if ((*it).instances.size() > 0) {
				for (PDH::Enumerations::Instances::const_iterator it2 = (*it).instances.begin();it2!=(*it).instances.end();++it2) {
					for (PDH::Enumerations::Counters::const_iterator it3 = (*it).counters.begin();it3!=(*it).counters.end();++it3) {
						std::string counter = "\\" + (*it).name + "(" + (*it2).name + ")\\" + (*it3).name;
						std::cout << "testing: " << counter << ": ";
						std::list<std::string> errors;
						std::list<std::string> status;
						std::string error;
						bool bStatus = true;
						if (PDH::Enumerations::validate(counter, error)) {
							status.push_back("open");
						} else {
							errors.push_back("NOT found: " + error);
							bStatus = false;
						}
						if (bStatus) {
							PDH::PDHCounter *pCounter = NULL;
							PDH::PDHQuery pdh;
							try {
								PDHCollectors::StaticPDHCounterListener<double, PDH_FMT_DOUBLE> cDouble;
								pCounter = pdh.addCounter(counter, &cDouble);
								pdh.open();

								if (pCounter != NULL) {
									try {
										PDH::PDHCounterInfo info = pCounter->getCounterInfo();
										errors.push_back("CounterName: " + info.szCounterName);
										errors.push_back("ExplainText: " + info.szExplainText);
										errors.push_back("FullPath: " + info.szFullPath);
										errors.push_back("InstanceName: " + info.szInstanceName);
										errors.push_back("MachineName: " + info.szMachineName);
										errors.push_back("ObjectName: " + info.szObjectName);
										errors.push_back("ParentInstance: " + info.szParentInstance);
										errors.push_back("Type: " + strEx::itos(info.dwType));
										errors.push_back("Scale: " + strEx::itos(info.lScale));
										errors.push_back("Default Scale: " + strEx::itos(info.lDefaultScale));
										errors.push_back("Status: " + strEx::itos(info.CStatus));
										status.push_back("described");
									} catch (const PDH::PDHException e) {
										errors.push_back("Describe failed: " + e.getError());
										bStatus = false;
									}
								}

								pdh.gatherData();
								pdh.close();
								status.push_back("queried");
							} catch (const PDH::PDHException e) {
								errors.push_back("Query failed: " + e.getError());
								bStatus = false;
								try {
									pdh.gatherData();
									pdh.close();
									bStatus = true;
								} catch (const PDH::PDHException e) {
									errors.push_back("Query failed (again!): " + e.getError());
								}
							}

						}
						if (!bStatus) {
							std::list<std::string>::const_iterator cit = status.begin();
							for (;cit != status.end(); ++cit) {
								std::cout << *cit << ", ";
							}
							std::cout << std::endl;
							std::cout << "  | Log" << std::endl;
							std::cout << "--+------  --    -" << std::endl;
							cit = errors.begin();
							for (;cit != errors.end(); ++cit) {
								std::cout << "  | " << *cit << std::endl;
							}
						} else {
							std::list<std::string>::const_iterator cit = status.begin();
							for (;cit != status.end(); ++cit) {
								std::cout << *cit << ", ";;
							}
							std::cout << std::endl;
						}
					}
				}
			} else {
				for (PDH::Enumerations::Counters::const_iterator it2 = (*it).counters.begin();it2!=(*it).counters.end();++it2) {
					std::string counter = "\\" + (*it).name + "\\" + (*it2).name;
					std::cout << "testing: " << counter << ": ";
					std::string error;
					if (PDH::Enumerations::validate(counter, error)) {
						std::cout << " found ";
					} else {
						std::cout << " *NOT* found (" << error << ") " << std::endl;
						break;
					}
					bool bOpend = false;
					try {
						PDH::PDHQuery pdh;
						PDHCollectors::StaticPDHCounterListener<double, PDH_FMT_DOUBLE> cDouble;
						pdh.addCounter(counter, &cDouble);
						pdh.open();
						pdh.gatherData();
						pdh.close();
						bOpend = true;
					} catch (const PDH::PDHException e) {
						std::cout << " could *not* be open (" << e.getError() << ") " << std::endl;
						break;
					}
					std::cout << " open ";
					std::cout << std::endl;;
				}
			}
		}
	} else if (_stricmp(command, "listpdh") == 0) {
		PDH::Enumerations::Objects lst;
		try {
			lst = PDH::Enumerations::EnumObjects();
		} catch (const PDH::PDHException e) {
			std::cout << "Service enumeration failed: " << e.getError();
			return 0;
		}
		for (PDH::Enumerations::Objects::iterator it = lst.begin();it!=lst.end();++it) {
			if ((*it).instances.size() > 0) {
				for (PDH::Enumerations::Instances::const_iterator it2 = (*it).instances.begin();it2!=(*it).instances.end();++it2) {
					for (PDH::Enumerations::Counters::const_iterator it3 = (*it).counters.begin();it3!=(*it).counters.end();++it3) {
						std::cout << "\\" << (*it).name << "(" << (*it2).name << ")\\" << (*it3).name << std::endl;;
					}
				}
			} else {
				for (PDH::Enumerations::Counters::const_iterator it2 = (*it).counters.begin();it2!=(*it).counters.end();++it2) {
					std::cout << "\\" << (*it).name << "\\" << (*it2).name << std::endl;;
				}
			}
		}
	}
	return 0;
}


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


class cpuload_handler {
public:
	static int parse(std::string s) {
		return strEx::stoi(s);
	}
	static int parse_percent(std::string s) {
		return strEx::stoi(s);
	}
	static std::string print(int value) {
		return strEx::itos(value) + "%";
	}
	static std::string print_unformated(int value) {
		return strEx::itos(value);
	}
	static std::string print_percent(int value) {
		return strEx::itos(value) + "%";
	}
	static std::string key_prefix() {
		return "average load ";
	}
	static std::string key_postfix() {
		return "";
	}
};
NSCAPI::nagiosReturn CheckSystem::checkCPU(const unsigned int argLen, char **char_args, std::string &msg, std::string &perf) 
{
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, cpuload_handler> > > CPULoadConatiner;

	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CPULoadConatiner> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	CPULoadConatiner tmpObject;

	tmpObject.data = "cpuload";

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_NUMERIC_ALL(tmpObject, "")
		MAP_OPTIONS_STR("warn", tmpObject.warn.max)
		MAP_OPTIONS_STR("crit", tmpObject.crit.max)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR_AND("time", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND("Time", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
			MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			else if (p2.first == "Time") {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				list.push_back(tmpObject);
			}
			MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
			MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_FALLBACK_AND(tmpObject.data, list.push_back(tmpObject))
	MAP_OPTIONS_END()

	for (std::list<CPULoadConatiner>::const_iterator it = list.begin(); it != list.end(); ++it) {
		CPULoadConatiner load = (*it);
		PDHCollector *pObject = pdhThread.getThread();
		if (!pObject) {
			msg = "ERROR: PDH Collection thread not running.";
			return NSCAPI::returnUNKNOWN;
		}
		int value = pObject->getCPUAvrage(load.data + "m");
		if (value == -1) {
			msg = "ERROR: We don't collect data this far back: " + load.getAlias();
			return NSCAPI::returnUNKNOWN;
		}
		if (bNSClient) {
			if (!msg.empty()) msg += "&";
			msg += strEx::itos(value);
		} else {
			load.setDefault(tmpObject);
			load.perfData = bPerfData;
			load.runCheck(value, returnCode, msg, perf);
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
	bool bNSClient = false;
	bool bPerfData = true;
	UpTimeConatiner bounds;

	bounds.data = "uptime";

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_NUMERIC_ALL(bounds, "")
		MAP_OPTIONS_STR("warn", bounds.warn.min)
		MAP_OPTIONS_STR("crit", bounds.crit.min)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR("Alias", bounds.data)
		MAP_OPTIONS_SHOWALL(bounds)
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
		bounds.perfData = bPerfData;
		bounds.runCheck(value, returnCode, msg, perf);
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
	std::set<std::string> excludeList;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	StateConatiner tmpObject;
	bool bPerfData = true;
	bool bAutoStart = false;

	tmpObject.data = "service";
	tmpObject.crit.state = "started";
	//{{
	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_STR("Alias", tmpObject.data)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE("CheckAll", bAutoStart)
		MAP_OPTIONS_INSERT("exclude", excludeList)
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
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
	//}}
	if (bAutoStart) {
		// get a list of all service with startup type Automatic 
		std::list<TNtServiceInfo> service_list_automatic;
		TNtServiceInfo::EnumServices(SERVICE_WIN32,SERVICE_INACTIVE|SERVICE_ACTIVE,&service_list_automatic); 
		for (std::list<TNtServiceInfo>::const_iterator service =service_list_automatic.begin();service!=service_list_automatic.end();++service) { 
			if (excludeList.find((*service).m_strServiceName) == excludeList.end()) {
				if((*service).m_dwStartType == 2 ) {
					tmpObject.data = (*service).m_strServiceName;
					tmpObject.crit.state = "started"; 
					list.push_back(tmpObject); 
					//stl_forward.push_back((*service).m_strServiceName); 
				}
				else if((*service).m_dwStartType == 4 ) {
					tmpObject.data = (*service).m_strServiceName;
					tmpObject.crit.state = "stopped"; 
					list.push_back(tmpObject); 
				}
			}
		} 
	}
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
			if ((info.m_dwCurrentState == SERVICE_RUNNING) && (*it).showAll()) {
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
			(*it).perfData = bPerfData;
			(*it).runCheck(value, returnCode, msg, perf);
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
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBounds<checkHolders::NumericPercentageBounds<checkHolders::PercentageValueType<unsigned __int64, unsigned __int64>, checkHolders::disk_size_handler<unsigned __int64> > > > MemoryConatiner;
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = "ERROR: Missing argument exception.";
		return NSCAPI::returnUNKNOWN;
	}
	std::list<MemoryConatiner> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bPerfData = true;
	bool bNSClient = false;
	MemoryConatiner tmpObject;

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_STR_AND("type", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND("Type", tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			MAP_OPTIONS_SECONDARY_STR_AND(p2,"type", tmpObject.data, tmpObject.alias, list.push_back(tmpObject))
			MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_DISK_ALL(tmpObject, "", "Free", "Used")
		MAP_OPTIONS_STR("Alias", tmpObject.data)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_MISSING(msg, "Unknown argument: ")
	MAP_OPTIONS_END()

	if (bNSClient) {
		tmpObject.data = "paged";
		list.push_back(tmpObject);
	}

	checkHolders::PercentageValueType<unsigned long long, unsigned long long> dataPaged;
	CheckMemory::memData data;
	bool firstPaged = true;
	bool firstMem = true;
	for (std::list<MemoryConatiner>::const_iterator pit = list.begin(); pit != list.end(); ++pit) {
		MemoryConatiner check = (*pit);
		check.setDefault(tmpObject);
		checkHolders::PercentageValueType<unsigned long long, unsigned long long> value;
		if (firstPaged && (check.data == "paged")) {
			firstPaged = false;
			PDHCollector *pObject = pdhThread.getThread();
			if (!pObject) {
				msg = "ERROR: PDH Collection thread not running.";
				return NSCAPI::returnUNKNOWN;
			}
			dataPaged.value = pObject->getMemCommit();
			dataPaged.total = pObject->getMemCommitLimit();
		} else if (firstMem) {
			try {
				data = memoryChecker.getMemoryStatus();
			} catch (CheckMemoryException e) {
				msg = e.getError() + ":" + strEx::itos(e.getErrorCode());
				return NSCAPI::returnCRIT;
			}
		}

		if (check.data == "page") {
			value.value = data.pageFile.total-data.pageFile.avail; // mem.dwTotalPageFile-mem.dwAvailPageFile;
			value.total = data.pageFile.total; //mem.dwTotalPageFile;
			if (check.alias.empty())
				check.alias = "page file";
		} else if (check.data == "physical") {
			value.value = data.phys.total-data.phys.avail; //mem.dwTotalPhys-mem.dwAvailPhys;
			value.total = data.phys.total; //mem.dwTotalPhys;
			if (check.alias.empty())
				check.alias = "physical memory";
		} else if (check.data == "virtual") {
			value.value = data.virtualMem.total-data.virtualMem.avail;//mem.dwTotalVirtual-mem.dwAvailVirtual;
			value.total = data.virtualMem.total;//mem.dwTotalVirtual;
			if (check.alias.empty())
				check.alias = "virtual memory";
		} else  if (check.data == "paged") {
			value.value = dataPaged.value;
			value.total = dataPaged.total;
			if (check.alias.empty())
				check.alias = "paged bytes";
		} else {
			msg = check.data + " is not a known check...";
			return NSCAPI::returnCRIT;
		}
		if (bNSClient) {
			msg = strEx::itos(value.total) + "&" + strEx::itos(value.value);
			return NSCAPI::returnOK;
		} else {
			check.perfData = bPerfData;
			check.runCheck(value, returnCode, msg, perf);
		}
	}
	NSC_DEBUG_MSG_STD("Perf data: " + strEx::itos(bPerfData) + ":" + perf);

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
typedef std::map<std::string,NSPROCDATA,strEx::case_blind_string_compare> NSPROCLST;
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
	bool bNSClient = false;
	StateConatiner tmpObject;
	bool bPerfData = true;

	tmpObject.data = "uptime";
	tmpObject.crit.state = "started";

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_NUMERIC_ALL(tmpObject, "Count")
		MAP_OPTIONS_STR("Alias", tmpObject.alias)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
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
			if (bFound && (*it).showAll()) {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": Running";
			} else if (bFound) {
			} else {
				if (!msg.empty()) msg += " - ";
				msg += (*it).data + ": not running";
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
			(*it).perfData = bPerfData;
			(*it).runCheck(value, returnCode, msg, perf);
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
	bool bNSClient = false;
	bool bPerfData = true;
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
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR("Alias", tmpObject.data)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_EX("Averages", bCheckAverages, "true", "false")
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_FIRST_CHAR('\\', tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_SECONDARY_BEGIN(":", p2)
			else if (p2.first == "Counter") {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				counters.push_back(tmpObject);
			}
			MAP_OPTIONS_MISSING_EX(p2, msg, "Unknown argument: ")
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_FALLBACK_AND(tmpObject.data, counters.push_back(tmpObject))
	MAP_OPTIONS_END()
	for (std::list<CounterConatiner>::const_iterator cit = counters.begin(); cit != counters.end(); ++cit) {
		CounterConatiner counter = (*cit);
		try {
			std::string tstr;
			if (!PDH::Enumerations::validate(counter.data, tstr)) {
				msg = tstr;
				msg += " (" + counter.getAlias() + "|" + counter.data + ")";
				return NSCAPI::returnUNKNOWN;
			}
			PDH::PDHQuery pdh;
			PDHCollectors::StaticPDHCounterListener<double, PDH_FMT_DOUBLE> cDouble;
			pdh.addCounter(counter.data, &cDouble);
			pdh.open();
			if (bCheckAverages) {
				pdh.collect();
				Sleep(1000);
			}
			pdh.gatherData();
			pdh.close();
			double value = cDouble.getValue();
			std::cout << "Collected double data: " << value << std::endl;
			if (bNSClient) {
				msg += strEx::itos(value);
			} else {
				counter.perfData = bPerfData;
				counter.setDefault(tmpObject);
				counter.runCheck(value, returnCode, msg, perf);
			}
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD("ERROR: " + e.getError() + " (" + counter.getAlias() + "|" + counter.data + ")");
			msg = static_cast<std::string>("ERROR: ") + e.getError()+ " (" + counter.getAlias() + "|" + counter.data + ")";
			return NSCAPI::returnUNKNOWN;
		}
	}

	if (msg.empty())
		msg = "OK all counters within bounds.";
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + ": " + msg;
	return returnCode;
}
NSC_WRAPPERS_MAIN_DEF(gCheckSystem);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckSystem);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gCheckSystem);
NSC_WRAPPERS_CLI_DEF(gCheckSystem);



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