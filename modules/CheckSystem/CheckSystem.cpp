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
#ifdef USE_BOOST
#include <boost/regex.hpp>
#endif

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
CheckSystem::CheckSystem() : pdhThread(_T("pdhThread")) {}
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
	try {
		NSCModuleHelper::registerCommand(_T("checkCPU"), _T("Check the CPU load of the computer."));
		NSCModuleHelper::registerCommand(_T("checkUpTime"), _T("Check the up-time of the computer."));
		NSCModuleHelper::registerCommand(_T("checkServiceState"), _T("Check the state of one or more of the computer services."));
		NSCModuleHelper::registerCommand(_T("checkProcState"), _T("Check the state of one or more of the processes running on the computer."));
		NSCModuleHelper::registerCommand(_T("checkMem"), _T("Check free/used memory on the system."));
		NSCModuleHelper::registerCommand(_T("checkCounter"), _T("Check a PDH counter."));
		NSCModuleHelper::registerCommand(_T("listCounterInstances"), _T("List all instances for a counter."));
		
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
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
		std::wcout << _T("MAJOR ERROR: Could not unload thread...") << std::endl;
		NSC_LOG_ERROR(_T("Could not exit the thread, memory leak and potential corruption may be the result..."));
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

int CheckSystem::commandLineExec(const TCHAR* command,const unsigned int argLen,TCHAR** args) {
	if (command == NULL) {
		std::wcerr << _T("Usage: ... CheckSystem <command>") << std::endl;
		std::wcerr << _T("Commands: debugpdh, listpdh, pdhlookup, pdhmatch, pdhobject") << std::endl;
		return -1;
	}
	if (_wcsicmp(command, _T("debugpdh")) == 0) {
		PDH::Enumerations::Objects lst;
		try {
			lst = PDH::Enumerations::EnumObjects();
		} catch (const PDH::PDHException e) {
			std::wcout << _T("Service enumeration failed: ") << e.getError();
			return 0;
		}
		for (PDH::Enumerations::Objects::iterator it = lst.begin();it!=lst.end();++it) {
			if ((*it).instances.size() > 0) {
				for (PDH::Enumerations::Instances::const_iterator it2 = (*it).instances.begin();it2!=(*it).instances.end();++it2) {
					for (PDH::Enumerations::Counters::const_iterator it3 = (*it).counters.begin();it3!=(*it).counters.end();++it3) {
						std::wstring counter = _T("\\") + (*it).name + _T("(") + (*it2).name + _T(")\\") + (*it3).name;
						std::wcout << _T("testing: ") << counter << _T(": ");
						std::list<std::wstring> errors;
						std::list<std::wstring> status;
						std::wstring error;
						bool bStatus = true;
						if (PDH::Enumerations::validate(counter, error)) {
							status.push_back(_T("open"));
						} else {
							errors.push_back(_T("NOT found: ") + error);
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
										errors.push_back(_T("CounterName: ") + info.szCounterName);
										errors.push_back(_T("ExplainText: ") + info.szExplainText);
										errors.push_back(_T("FullPath: ") + info.szFullPath);
										errors.push_back(_T("InstanceName: ") + info.szInstanceName);
										errors.push_back(_T("MachineName: ") + info.szMachineName);
										errors.push_back(_T("ObjectName: ") + info.szObjectName);
										errors.push_back(_T("ParentInstance: ") + info.szParentInstance);
										errors.push_back(_T("Type: ") + strEx::itos(info.dwType));
										errors.push_back(_T("Scale: ") + strEx::itos(info.lScale));
										errors.push_back(_T("Default Scale: ") + strEx::itos(info.lDefaultScale));
										errors.push_back(_T("Status: ") + strEx::itos(info.CStatus));
										status.push_back(_T("described"));
									} catch (const PDH::PDHException e) {
										errors.push_back(_T("Describe failed: ") + e.getError());
										bStatus = false;
									}
								}

								pdh.gatherData();
								pdh.close();
								status.push_back(_T("queried"));
							} catch (const PDH::PDHException e) {
								errors.push_back(_T("Query failed: ") + e.getError());
								bStatus = false;
								try {
									pdh.gatherData();
									pdh.close();
									bStatus = true;
								} catch (const PDH::PDHException e) {
									errors.push_back(_T("Query failed (again!): ") + e.getError());
								}
							}

						}
						if (!bStatus) {
							std::list<std::wstring>::const_iterator cit = status.begin();
							for (;cit != status.end(); ++cit) {
								std::wcout << *cit << _T(", ");
							}
							std::wcout << std::endl;
							std::wcout << _T("  | Log") << std::endl;
							std::wcout << _T("--+------  --    -") << std::endl;
							cit = errors.begin();
							for (;cit != errors.end(); ++cit) {
								std::wcout << _T("  | ") << *cit << std::endl;
							}
						} else {
							std::list<std::wstring>::const_iterator cit = status.begin();
							for (;cit != status.end(); ++cit) {
								std::wcout << *cit << _T(", ");;
							}
							std::wcout << std::endl;
						}
					}
				}
			} else {
				for (PDH::Enumerations::Counters::const_iterator it2 = (*it).counters.begin();it2!=(*it).counters.end();++it2) {
					std::wstring counter = _T("\\") + (*it).name + _T("\\") + (*it2).name;
					std::wcout << _T("testing: ") << counter << _T(": ");
					std::wstring error;
					if (PDH::Enumerations::validate(counter, error)) {
						std::wcout << _T(" found ");
					} else {
						std::wcout << _T(" *NOT* found (") << error << _T(") ") << std::endl;
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
						std::wcout << _T(" could *not* be open (") << e.getError() << _T(") ") << std::endl;
						break;
					}
					std::wcout << _T(" open ");
					std::wcout << std::endl;;
				}
			}
		}
	} else if (_wcsicmp(command, _T("listpdh")) == 0) {
		PDH::Enumerations::Objects lst;
		try {
			lst = PDH::Enumerations::EnumObjects();
		} catch (const PDH::PDHException e) {
			std::wcout << _T("Service enumeration failed: ") << e.getError();
			return 0;
		}
		for (PDH::Enumerations::Objects::iterator it = lst.begin();it!=lst.end();++it) {
			if ((*it).instances.size() > 0) {
				for (PDH::Enumerations::Instances::const_iterator it2 = (*it).instances.begin();it2!=(*it).instances.end();++it2) {
					for (PDH::Enumerations::Counters::const_iterator it3 = (*it).counters.begin();it3!=(*it).counters.end();++it3) {
						std::wcout << _T("\\") << (*it).name << _T("(") << (*it2).name << _T(")\\") << (*it3).name << std::endl;;
					}
				}
			} else {
				for (PDH::Enumerations::Counters::const_iterator it2 = (*it).counters.begin();it2!=(*it).counters.end();++it2) {
					std::wcout << _T("\\") << (*it).name << _T("\\") << (*it2).name << std::endl;;
				}
			}
		}
	} else if (_wcsicmp(command, _T("pdhlookup")) == 0) {
		try {
			std::wstring name = arrayBuffer::arrayBuffer2string(args, argLen, _T(" "));
			if (name.empty()) {
				NSC_LOG_ERROR_STD(_T("Need to specify counter index name!"));
				return 0;
			}
			DWORD dw = PDH::PDHQuery::lookupIndex(name);
			NSC_LOG_MESSAGE_STD(_T("--+--[ Lookup Result ]----------------------------------------"));
			NSC_LOG_MESSAGE_STD(_T("  | Index for '") + name + _T("' is ") + strEx::itos(dw));
			NSC_LOG_MESSAGE_STD(_T("--+-----------------------------------------------------------"));
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("Failed to lookup index: ") + e.getError());
			return 0;
		}
	} else if (_wcsicmp(command, _T("pdhmatch")) == 0) {
		try {
			std::wstring name = arrayBuffer::arrayBuffer2string(args, argLen, _T(" "));
			if (name.empty()) {
				NSC_LOG_ERROR_STD(_T("Need to specify counter pattern!"));
				return 0;
			}
			std::list<std::wstring> list = PDH::PDHResolver::PdhExpandCounterPath(name.c_str());
			NSC_LOG_MESSAGE_STD(_T("--+--[ Lookup Result ]----------------------------------------"));
			for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
				NSC_LOG_MESSAGE_STD(_T("  | Found '") + *cit);
			}
			NSC_LOG_MESSAGE_STD(_T("--+-----------------------------------------------------------"));
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("Failed to lookup index: ") + e.getError());
			return 0;
		}
	} else if (_wcsicmp(command, _T("pdhobject")) == 0) {
		try {
			std::wstring name = arrayBuffer::arrayBuffer2string(args, argLen, _T(" "));
			if (name.empty()) {
				NSC_LOG_ERROR_STD(_T("Need to specify counter pattern!"));
				return 0;
			}
			PDH::Enumerations::pdh_object_details list = PDH::Enumerations::EnumObjectInstances(name.c_str());
			NSC_LOG_MESSAGE_STD(_T("--+--[ Lookup Result ]----------------------------------------"));
			for (std::list<std::wstring>::const_iterator cit = list.counters.begin(); cit != list.counters.end(); ++cit) {
				NSC_LOG_MESSAGE_STD(_T("  | Found Counter: ") + *cit);
			}
			for (std::list<std::wstring>::const_iterator cit = list.instances.begin(); cit != list.instances.end(); ++cit) {
				NSC_LOG_MESSAGE_STD(_T("  | Found Instance: ") + *cit);
			}
			NSC_LOG_MESSAGE_STD(_T("--+-----------------------------------------------------------"));
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("Failed to lookup index: ") + e.getError());
			return 0;
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
NSCAPI::nagiosReturn CheckSystem::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	std::list<std::wstring> stl_args;
	CheckSystem::returnBundle rb;
	if (command == _T("checkCPU")) {
		return checkCPU(argLen, char_args, msg, perf);
	} else if (command == _T("checkUpTime")) {
		return checkUpTime(argLen, char_args, msg, perf);
	} else if (command == _T("checkServiceState")) {
		return checkServiceState(argLen, char_args, msg, perf);
	} else if (command == _T("checkProcState")) {
		return checkProcState(argLen, char_args, msg, perf);
	} else if (command == _T("checkMem")) {
		return checkMem(argLen, char_args, msg, perf);
	} else if (command == _T("checkCounter")) {
		return checkCounter(argLen, char_args, msg, perf);
	} else if (command == _T("listCounterInstances")) {
		return listCounterInstances(argLen, char_args, msg, perf);
	}
	return NSCAPI::returnIgnored;
}


class cpuload_handler {
public:
	static int parse(std::wstring s) {
		return strEx::stoi(s);
	}
	static int parse_percent(std::wstring s) {
		return strEx::stoi(s);
	}
	static std::wstring print(int value) {
		return strEx::itos(value) + _T("%");
	}
	static std::wstring print_unformated(int value) {
		return strEx::itos(value);
	}

	static std::wstring get_perf_unit(__int64 value) {
		return _T("%");
	}
	static std::wstring print_perf(__int64 value, std::wstring unit) {
		return strEx::itos(value);
	}
	static std::wstring print_percent(int value) {
		return strEx::itos(value) + _T("%");
	}
	static std::wstring key_prefix() {
		return _T("average load ");
	}
	static std::wstring key_postfix() {
		return _T("");
	}
};
NSCAPI::nagiosReturn CheckSystem::checkCPU(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) 
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, cpuload_handler> > > CPULoadContainer;

	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CPULoadContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	CPULoadContainer tmpObject;

	tmpObject.data = _T("cpuload");

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_NUMERIC_ALL(tmpObject, _T(""))
		MAP_OPTIONS_STR(_T("warn"), tmpObject.warn.max)
		MAP_OPTIONS_STR(_T("crit"), tmpObject.crit.max)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR_AND(_T("time"), tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND(_T("Time"), tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
			else if (p2.first == _T("Time")) {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				list.push_back(tmpObject);
			}
	MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_FALLBACK_AND(tmpObject.data, list.push_back(tmpObject))
	MAP_OPTIONS_END()

	for (std::list<CPULoadContainer>::const_iterator it = list.begin(); it != list.end(); ++it) {
		CPULoadContainer load = (*it);
		PDHCollector *pObject = pdhThread.getThread();
		if (!pObject) {
			msg = _T("ERROR: PDH Collection thread not running.");
			return NSCAPI::returnUNKNOWN;
		}
		int value = pObject->getCPUAvrage(load.data + _T("m"));
		if (value == -1) {
			msg = _T("ERROR: Could not get data for ") + load.getAlias() + _T(" perhaps we don't collect data this far back?");
			return NSCAPI::returnUNKNOWN;
		}
		if (bNSClient) {
			if (!msg.empty()) msg += _T("&");
			msg += strEx::itos(value);
		} else {
			load.setDefault(tmpObject);
			load.perfData = bPerfData;
			load.runCheck(value, returnCode, msg, perf);
		}
	}

	if (msg.empty())
		msg = _T("OK CPU Load ok.");
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}

NSCAPI::nagiosReturn CheckSystem::checkUpTime(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsTime> UpTimeContainer;

	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	UpTimeContainer bounds;

	bounds.data = _T("uptime");

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_NUMERIC_ALL(bounds, _T(""))
		MAP_OPTIONS_STR(_T("warn"), bounds.warn.min)
		MAP_OPTIONS_STR(_T("crit"), bounds.crit.min)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR(_T("Alias"), bounds.data)
		MAP_OPTIONS_SHOWALL(bounds)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_MISSING(msg, _T("Unknown argument: "))
		MAP_OPTIONS_END()


	PDHCollector *pObject = pdhThread.getThread();
	if (!pObject) {
		msg = _T("ERROR: PDH Collection thread not running.");
		return NSCAPI::returnUNKNOWN;
	}
	unsigned long long value = pObject->getUptime();
	if (value == -1) {
		msg = _T("ERROR: Could not get value");
		return NSCAPI::returnUNKNOWN;
	}
	if (bNSClient) {
		msg = strEx::itos(value);
	} else {
		value *= 1000;
		bounds.perfData = bPerfData;
		bounds.runCheck(value, returnCode, msg, perf);
	}

	if (msg.empty())
		msg = _T("OK all counters within bounds.");
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}

// @todo state_handler


inline int get_state(DWORD state) {
	if (state == SERVICE_RUNNING)
		return checkHolders::state_started;
	else if (state == SERVICE_STOPPED)
		return checkHolders::state_stopped;
	else if (state == MY_SERVICE_NOT_FOUND)
		return checkHolders::state_not_found;
	return checkHolders::state_none;
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
 * @param command Command to execute
 * @param argLen The length of the argument buffer
 * @param **char_args The argument buffer
 * @param &msg String to put message in
 * @param &perf String to put performance data in 
 * @return The status of the command
 */
NSCAPI::nagiosReturn CheckSystem::checkServiceState(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::SimpleBoundsStateBoundsInteger> StateContainer;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<StateContainer> list;
	std::set<std::wstring> excludeList;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	StateContainer tmpObject;
	bool bPerfData = true;
	bool bAutoStart = false;
	unsigned int truncate = 0;

	tmpObject.data = _T("service");
	tmpObject.crit.state = _T("started");
	//{{
	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
		MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE(_T("CheckAll"), bAutoStart)
		MAP_OPTIONS_INSERT(_T("exclude"), excludeList)
		//MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		//MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		//MAP_OPTIONS_SECONDARY_END()
		else { 
			tmpObject.data = p__.first;
			if (p__.second.empty())
				tmpObject.crit.state = _T("started"); 
			else
				tmpObject.crit.state = p__.second; 
			list.push_back(tmpObject); 
		}
	MAP_OPTIONS_END()
	//}}
	if (bAutoStart) {
		// get a list of all service with startup type Automatic 
		/*
		;check_all_services[SERVICE_BOOT_START]=ignored
		;check_all_services[SERVICE_SYSTEM_START]=ignored
		;check_all_services[SERVICE_AUTO_START]=started
		;check_all_services[SERVICE_DEMAND_START]=ignored
		;check_all_services[SERVICE_DISABLED]=stopped
		std::wstring wantedMethod = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_ENUMPROC_METHOD, C_SYSTEM_ENUMPROC_METHOD_DEFAULT);
		*/
		std::map<DWORD,std::wstring> lookups;
		lookups[SERVICE_BOOT_START] = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_SVC_ALL_0, C_SYSTEM_SVC_ALL_0_DEFAULT);
		lookups[SERVICE_SYSTEM_START] = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_SVC_ALL_1, C_SYSTEM_SVC_ALL_1_DEFAULT);
		lookups[SERVICE_AUTO_START] = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_SVC_ALL_2, C_SYSTEM_SVC_ALL_2_DEFAULT);
		lookups[SERVICE_DEMAND_START] = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_SVC_ALL_3, C_SYSTEM_SVC_ALL_3_DEFAULT);
		lookups[SERVICE_DISABLED] = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_SVC_ALL_4, C_SYSTEM_SVC_ALL_4_DEFAULT);


		std::list<TNtServiceInfo> service_list_automatic = TNtServiceInfo::EnumServices(SERVICE_WIN32,SERVICE_INACTIVE|SERVICE_ACTIVE); 
		for (std::list<TNtServiceInfo>::const_iterator service =service_list_automatic.begin();service!=service_list_automatic.end();++service) { 
			if (excludeList.find((*service).m_strServiceName) == excludeList.end()) {
				tmpObject.data = (*service).m_strServiceName;
				tmpObject.crit.state = lookups[(*service).m_dwStartType]; 
				list.push_back(tmpObject); 
			}
		} 
		tmpObject.crit.state = _T("ignored");
	}
	for (std::list<StateContainer>::iterator it = list.begin(); it != list.end(); ++it) {
		TNtServiceInfo info;
		if (bNSClient) {
			try {
				info = TNtServiceInfo::GetService((*it).data.c_str());
			} catch (NTServiceException e) {
				if (!msg.empty()) msg += _T(" - ");
				msg += (*it).data + _T(": Error");
				NSCHelper::escalteReturnCodeToWARN(returnCode);
				continue;
			}
			if ((*it).crit.state.hasBounds()) {
				bool ok = (*it).crit.state.check(get_state(info.m_dwCurrentState));
				if (!ok || (*it).showAll()) {
					if (info.m_dwCurrentState == SERVICE_RUNNING) {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Started");
					} else if (info.m_dwCurrentState == SERVICE_STOPPED) {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Stopped");
					} else if (info.m_dwCurrentState == MY_SERVICE_NOT_FOUND) {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Not found");
					} else {
						if (!msg.empty()) msg += _T(" - ");
						msg += (*it).data + _T(": Unknown");
					}
					if (!ok) 
						NSCHelper::escalteReturnCodeToCRIT(returnCode);
				}
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
			else if (info.m_dwCurrentState == MY_SERVICE_NOT_FOUND)
				value = checkHolders::state_not_found;
			else {
				NSC_LOG_MESSAGE(_T("Service had no (valid) state: ") + (*it).data + _T(" (") + strEx::itos(info.m_dwCurrentState) + _T(")"));
				value = checkHolders::state_none;
			}
			unsigned int x = returnCode;
			(*it).perfData = bPerfData;
			(*it).setDefault(tmpObject);
			(*it).runCheck(value, returnCode, msg, perf);
//			NSC_LOG_MESSAGE(_T("Service: ") + (*it).data + _T(" (") + strEx::itos(info.m_dwCurrentState) + _T(":") + strEx::itos((*it).warn.state.value_) + _T(":") + strEx::itos((*it).crit.state.value_) + _T(") -- (") + strEx::itos(returnCode) + _T(":") + strEx::itos(x) + _T(")"));
		}
	}
	if ((truncate > 0) && (msg.length() > (truncate-4)))
		msg = msg.substr(0, truncate-4) + _T("...");
	if (msg.empty() && returnCode == NSCAPI::returnOK)
		msg = _T("OK: All services are in their appropriate state.");
	else if (msg.empty())
		msg = NSCHelper::translateReturn(returnCode) + _T(": Whooha this is odd.");
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + _T(": ") + msg;
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
NSCAPI::nagiosReturn CheckSystem::checkMem(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericPercentageBounds<checkHolders::PercentageValueType<unsigned __int64, unsigned __int64>, checkHolders::disk_size_handler<unsigned __int64> > > > MemoryContainer;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<MemoryContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bPerfData = true;
	bool bNSClient = false;
	MemoryContainer tmpObject;

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_STR_AND(_T("type"), tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_STR_AND(_T("Type"), tmpObject.data, list.push_back(tmpObject))
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		MAP_OPTIONS_SECONDARY_STR_AND(p2,_T("type"), tmpObject.data, tmpObject.alias, list.push_back(tmpObject))
			MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
			MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_DISK_ALL(tmpObject, _T(""), _T("Free"), _T("Used"))
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_MISSING(msg, _T("Unknown argument: "))
		MAP_OPTIONS_END()

	if (bNSClient) {
		tmpObject.data = _T("paged");
		list.push_back(tmpObject);
	}

	checkHolders::PercentageValueType<unsigned long long, unsigned long long> dataPaged;
	CheckMemory::memData data;
	bool firstPaged = true;
	bool firstMem = true;
	for (std::list<MemoryContainer>::const_iterator pit = list.begin(); pit != list.end(); ++pit) {
		MemoryContainer check = (*pit);
		check.setDefault(tmpObject);
		checkHolders::PercentageValueType<unsigned long long, unsigned long long> value;
		if (firstPaged && (check.data == _T("paged"))) {
			firstPaged = false;
			PDHCollector *pObject = pdhThread.getThread();
			if (!pObject) {
				msg = _T("ERROR: PDH Collection thread not running.");
				return NSCAPI::returnUNKNOWN;
			}
			dataPaged.value = pObject->getMemCommit();
			if (dataPaged.value == -1) {
				msg = _T("ERROR: Failed to get PDH value.");
				return NSCAPI::returnUNKNOWN;
			}
			dataPaged.total = pObject->getMemCommitLimit();
			if (dataPaged.total == -1) {
				msg = _T("ERROR: Failed to get PDH value.");
				return NSCAPI::returnUNKNOWN;
			}
		} else if (firstMem) {
			try {
				data = memoryChecker.getMemoryStatus();
			} catch (CheckMemoryException e) {
				msg = e.getError();
				return NSCAPI::returnCRIT;
			}
		}

		if (check.data == _T("page")) {
			value.value = data.pageFile.total-data.pageFile.avail; // mem.dwTotalPageFile-mem.dwAvailPageFile;
			value.total = data.pageFile.total; //mem.dwTotalPageFile;
			if (check.alias.empty())
				check.alias = _T("page file");
		} else if (check.data == _T("physical")) {
			value.value = data.phys.total-data.phys.avail; //mem.dwTotalPhys-mem.dwAvailPhys;
			value.total = data.phys.total; //mem.dwTotalPhys;
			if (check.alias.empty())
				check.alias = _T("physical memory");
		} else if (check.data == _T("virtual")) {
			value.value = data.virtualMem.total-data.virtualMem.avail;//mem.dwTotalVirtual-mem.dwAvailVirtual;
			value.total = data.virtualMem.total;//mem.dwTotalVirtual;
			if (check.alias.empty())
				check.alias = _T("virtual memory");
		} else  if (check.data == _T("paged")) {
			value.value = dataPaged.value;
			value.total = dataPaged.total;
			if (check.alias.empty())
				check.alias = _T("paged bytes");
		} else {
			msg = check.data + _T(" is not a known check...");
			return NSCAPI::returnCRIT;
		}
		if (bNSClient) {
			msg = strEx::itos(value.total) + _T("&") + strEx::itos(value.value);
			return NSCAPI::returnOK;
		} else {
			check.perfData = bPerfData;
			check.runCheck(value, returnCode, msg, perf);
		}
	}

	if (msg.empty())
		msg = _T("OK memory within bounds.");
	else
		msg = NSCHelper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}
typedef struct NSPROCDATA__ {
	unsigned int count;
	CEnumProcess::CProcessEntry entry;
	std::wstring key;

	NSPROCDATA__() : count(0) {}
	NSPROCDATA__(const NSPROCDATA__ &other) : count(other.count), entry(other.entry), key(other.key) {}
} NSPROCDATA;
typedef std::map<std::wstring,NSPROCDATA,strEx::case_blind_string_compare> NSPROCLST;

class NSC_error : public CEnumProcess::error_reporter {
	void report_error(std::wstring error) {
		NSC_LOG_ERROR(error);
	}
	void report_warning(std::wstring error) {
		NSC_LOG_MESSAGE(error);
	}
	void report_debug(std::wstring error) {
		NSC_DEBUG_MSG_STD(_T("PROC::: ") + error);
	}
	void report_debug_enter(std::wstring error) {
		NSC_DEBUG_MSG_STD(_T("PROC>>> ") + error);
	}
	void report_debug_exit(std::wstring error) {
		NSC_DEBUG_MSG_STD(_T("PROC<<<") + error);
	}
};

/**
* Get a hash_map with all running processes.
* @return a hash_map with all running processes
*/
NSPROCLST GetProcessList(bool getCmdLines, bool use16Bit)
{
	NSPROCLST ret;
	CEnumProcess enumeration;
	if (!enumeration.has_PSAPI()) {
		NSC_LOG_ERROR_STD(_T("Failed to enumerat processes"));
		NSC_LOG_ERROR_STD(_T("PSAPI method not availabletry installing \"Platform SDK Redistributable: PSAPI for Windows NT\" from Microsoft."));
		NSC_LOG_ERROR_STD(_T("Try this URL: http://www.microsoft.com/downloads/details.aspx?FamilyID=3d1fbaed-d122-45cf-9d46-1cae384097ac"));
		throw CEnumProcess::process_enumeration_exception(_T("PSAPI not avalible, please see eror log for details."));
	}
	NSC_error err;
	CEnumProcess::process_list list = enumeration.enumerate_processes(getCmdLines, use16Bit, &err);
	for (CEnumProcess::process_list::const_iterator entry = list.begin(); entry != list.end(); ++entry) {
		std::wstring key;
		if (getCmdLines && !(*entry).command_line.empty())
			key = (*entry).command_line;
		else
			key = (*entry).filename;
		NSPROCLST::iterator it = ret.find(key);
		if (it == ret.end()) {
			ret[key].entry = (*entry);
			ret[key].count = 1;
			ret[key].key = key;
		} else
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
NSCAPI::nagiosReturn CheckSystem::checkProcState(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinStateBoundsStateBoundsInteger> StateContainer;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<StateContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	StateContainer tmpObject;
	bool bPerfData = true;
	bool use16bit = false;
	bool useCmdLine = false;
	typedef enum {
		match_string, match_substring, match_regexp
	} match_type;
	match_type match = match_string;

	

	tmpObject.data = _T("uptime");
	tmpObject.crit.state = _T("started");

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_NUMERIC_ALL(tmpObject, _T("Count"))
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.alias)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE(_T("cmdLine"), useCmdLine)
		MAP_OPTIONS_BOOL_TRUE(_T("16bit"), use16bit)
		MAP_OPTIONS_MODE(_T("match"), _T("string"), match,  match_string)
		MAP_OPTIONS_MODE(_T("match"), _T("regexp"), match,  match_regexp)
		MAP_OPTIONS_MODE(_T("match"), _T("substr"), match,  match_substring)
		MAP_OPTIONS_MODE(_T("match"), _T("substring"), match,  match_substring)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
	else if (p2.first == _T("Proc")) {
			tmpObject.data = p__.second;
			tmpObject.alias = p2.second;
			list.push_back(tmpObject);
		}
	MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		MAP_OPTIONS_SECONDARY_END()
		else { 
			tmpObject.data = p__.first;
			if (p__.second.empty())
				tmpObject.crit.state = _T("started"); 
			else
				tmpObject.crit.state = p__.second; 
			list.push_back(tmpObject); 
		}
	MAP_OPTIONS_END()

	NSPROCLST runningProcs;
	try {
		runningProcs = GetProcessList(useCmdLine, use16bit);
	} catch (CEnumProcess::process_enumeration_exception &e) {
		NSC_LOG_ERROR_STD(_T("ERROR: ") + e.what());
		msg = static_cast<std::wstring>(_T("ERROR: ")) + e.what();
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unhandled error when processing command"));
		msg = _T("Unhandled error when processing command");
		return NSCAPI::returnUNKNOWN;
	}

	for (std::list<StateContainer>::iterator it = list.begin(); it != list.end(); ++it) {
		NSPROCLST::iterator proc;
		if (match == match_string) {
			proc = runningProcs.find((*it).data);
		} else if (match == match_substring) {
			for (proc=runningProcs.begin();proc!=runningProcs.end();++proc) {
				if ((*proc).first.find((*it).data) != std::wstring::npos)
					break;
			}
#ifdef USE_BOOST
		} else if (match == match_regexp) {
			try {
				boost::wregex filter((*it).data,boost::regex::icase);
				for (proc=runningProcs.begin();proc!=runningProcs.end();++proc) {
					std::wstring value = (*proc).first;
					if (boost::regex_match(value, filter))
						break;
				}
			} catch (const boost::bad_expression e) {
				NSC_LOG_ERROR_STD(_T("Failed to compile regular expression: ") + (*proc).first);
				msg = _T("Failed to compile regular expression: ") + (*proc).first;
				return NSCAPI::returnUNKNOWN;
			} catch (...) {
				NSC_LOG_ERROR_STD(_T("Failed to compile regular expression: ") + (*proc).first);
				msg = _T("Failed to compile regular expression: ") + (*proc).first;
				return NSCAPI::returnUNKNOWN;
			}
#else
			NSC_LOG_ERROR_STD(_T("NSClient++ is compiled without USEBOOST so no regular expression support for you...") + (*proc).first);
			msg = _T("Regular expression is not supported: ") + (*proc).first;
			return NSCAPI::returnUNKNOWN;
#endif
		} else {
			NSC_LOG_ERROR_STD(_T("Unsupported mode for: ") + (*proc).first);
			msg = _T("Unsupported mode for: ") + (*proc).first;
			return NSCAPI::returnUNKNOWN;
		}
		bool bFound = (proc != runningProcs.end());
		if (bNSClient) {
			if (bFound && (*it).showAll()) {
				if (!msg.empty()) msg += _T(" - ");
				msg += (*proc).first + _T(": Running");
			} else if (bFound) {
			} else {
				if (!msg.empty()) msg += _T(" - ");
				msg += (*it).data + _T(": not running");
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
			if (bFound && (*it).alias.empty()) {
				(*it).alias = (*proc).first;
			}
			(*it).perfData = bPerfData;
			(*it).runCheck(value, returnCode, msg, perf);
		}

	}
	if (msg.empty())
		msg = _T("OK: All processes are running.");
	else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + _T(": ") + msg;
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
NSCAPI::nagiosReturn CheckSystem::checkCounter(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDouble> CounterContainer;

	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CounterContainer> counters;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	/* average maax */
	bool bCheckAverages = true; 
	std::wstring invalidStatus = _T("UNKNOWN");
	unsigned int averageDelay = 1000;
	CounterContainer tmpObject;

	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_STR(_T("InvalidStatus"), invalidStatus)
		MAP_OPTIONS_STR_AND(_T("Counter"), tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_STR(_T("MaxWarn"), tmpObject.warn.max)
		MAP_OPTIONS_STR(_T("MinWarn"), tmpObject.warn.min)
		MAP_OPTIONS_STR(_T("MaxCrit"), tmpObject.crit.max)
		MAP_OPTIONS_STR(_T("MinCrit"), tmpObject.crit.min)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_EX(_T("Averages"), bCheckAverages, _T("true"), _T("false"))
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_FIRST_CHAR('\\', tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
	else if (p2.first == _T("Counter")) {
		tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				counters.push_back(tmpObject);
			}
	MAP_OPTIONS_MISSING_EX(p2, msg, _T("Unknown argument: "))
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_FALLBACK_AND(tmpObject.data, counters.push_back(tmpObject))
	MAP_OPTIONS_END()
	for (std::list<CounterContainer>::const_iterator cit = counters.begin(); cit != counters.end(); ++cit) {
		CounterContainer counter = (*cit);
		try {
			std::wstring tstr;
			if (!PDH::Enumerations::validate(counter.data, tstr)) {
				NSC_LOG_ERROR_STD(_T("ERROR: Counter not found: ") + counter.data + _T(": ") + tstr);
				if (bNSClient) {
					NSC_LOG_ERROR_STD(_T("ERROR: Counter not found: ") + counter.data + _T(": ") + tstr);
					//msg = _T("0");
				} else {
					//msg = tstr;
					//msg += _T(" (") + counter.getAlias() + _T("|") + counter.data + _T(")");
				}
				//return NSCHelper::translateReturn(invalidStatus);
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
			if (bNSClient) {
				if (!msg.empty())
					msg += _T(",");
				msg += strEx::itos(static_cast<float>(value));
			} else {
				std::wcout << _T("perf data: ") << bPerfData << std::endl;
				counter.perfData = bPerfData;
				counter.setDefault(tmpObject);
				counter.runCheck(value, returnCode, msg, perf);
			}
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("ERROR: ") + e.getError() + _T(" (") + counter.getAlias() + _T("|") + counter.data + _T(")"));
			if (bNSClient)
				msg = _T("0");
			else
				msg = static_cast<std::wstring>(_T("ERROR: ")) + e.getError()+ _T(" (") + counter.getAlias() + _T("|") + counter.data + _T(")");
			return NSCAPI::returnUNKNOWN;
		}
	}

	if (msg.empty() && !bNSClient)
		msg = _T("OK all counters within bounds.");
	else if (msg.empty()) {
		NSC_LOG_ERROR_STD(_T("No value found returning 0?"));
		msg = _T("0");
	}else if (!bNSClient)
		msg = NSCHelper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}



/**
 * List all instances for a given counter.
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
NSCAPI::nagiosReturn CheckSystem::listCounterInstances(const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDouble> CounterContainer;

	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (stl_args.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}

	std::wstring counter = arrayBuffer::arrayBuffer2string(char_args, argLen, _T(" "));
	try {
		PDH::Enumerations::pdh_object_details obj = PDH::Enumerations::EnumObjectInstances(counter);
		for (PDH::Enumerations::pdh_object_details::list::const_iterator it = obj.instances.begin(); it!=obj.instances.end();++it) {
			if (!msg.empty())
				msg += _T(", ");
			msg += (*it);
		}
	} catch (const PDH::PDHException e) {
		msg = _T("ERROR: Failed to enumerate counter instances: " + e.getError());
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		msg = _T("ERROR: Failed to enumerate counter instances: <UNKNOWN EXCEPTION>");
		return NSCAPI::returnUNKNOWN;
	}
	return NSCAPI::returnOK;
}


NSC_WRAPPERS_MAIN_DEF(gCheckSystem);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckSystem);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gCheckSystem);
NSC_WRAPPERS_CLI_DEF(gCheckSystem);



MODULE_SETTINGS_START(CheckSystem, _T("System check module"), _T("..."))

PAGE(_T("Check options"))

ITEM_EDIT_TEXT(_T("Check resolution"), _T("This is how often the PDH data is polled and stored in the CPU buffer. (this is enterd in 1/th: of a second)"))
OPTION(_T("unit"), _T("1/10:th of a second"))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("Check System"))
OPTION(_T("key"), _T("CheckResolution"))
OPTION(_T("default"), _T("10"))
ITEM_END()

ITEM_EDIT_TEXT(_T("CPU buffer size"), _T("This is the size of the buffer that stores CPU history."))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("Check System"))
OPTION(_T("key"), _T("CPUBufferSize"))
OPTION(_T("default"), _T("1h"))
ITEM_END()

PAGE_END()
ADVANCED_PAGE(_T("Compatiblity settings"))

ITEM_EDIT_TEXT(_T("MemoryCommitByte"), _T("The memory commited bytes used to calculate the avalible memory."))
OPTION(_T("disableCaption"), _T("Attempt to autodetect this."))
OPTION(_T("disabled"), _T("auto"))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("Check System"))
OPTION(_T("key"), _T("MemoryCommitByte"))
OPTION(_T("default"), _T("auto"))
ITEM_END()

ITEM_EDIT_TEXT(_T("MemoryCommitLimit"), _T("The memory commit limit used to calculate the avalible memory."))
OPTION(_T("disableCaption"), _T("Attempt to autodetect this."))
OPTION(_T("disabled"), _T("auto"))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("Check System"))
OPTION(_T("key"), _T("MemoryCommitLimit"))
OPTION(_T("default"), _T("auto"))
ITEM_END()

ITEM_EDIT_TEXT(_T("SystemSystemUpTime"), _T("The PDH counter for the System uptime."))
OPTION(_T("disableCaption"), _T("Attempt to autodetect this."))
OPTION(_T("disabled"), _T("auto"))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("Check System"))
OPTION(_T("key"), _T("SystemSystemUpTime"))
OPTION(_T("default"), _T("auto"))
ITEM_END()

ITEM_EDIT_TEXT(_T("SystemTotalProcessorTime"), _T("The PDH conter usaed to measure CPU load."))
OPTION(_T("disableCaption"), _T("Attempt to autodetect this."))
OPTION(_T("disabled"), _T("auto"))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("Check System"))
OPTION(_T("key"), _T("SystemTotalProcessorTime"))
OPTION(_T("default"), _T("auto"))
ITEM_END()

ITEM_EDIT_TEXT(_T("ProcessEnumerationMethod"), _T("The method to use when enumerating processes"))
OPTION(_T("count"), _T("3"))
OPTION(_T("caption_1"), _T("Autodetect (TOOLHELP for NT/4 and PSAPI for W2k)"))
OPTION(_T("value_1"), _T("auto"))
OPTION(_T("caption_2"), _T("TOOLHELP use this for NT/4 systems"))
OPTION(_T("value_2"), _T("TOOLHELP"))
OPTION(_T("caption_3"), _T("PSAPI use this for W2k (and abowe) systems"))
OPTION(_T("value_3"), _T("PSAPI"))
ITEM_MAP_TO(_T("basic_ini_text_mapper"))
OPTION(_T("section"), _T("Check System"))
OPTION(_T("key"), _T("ProcessEnumerationMethod"))
OPTION(_T("default"), _T("auto"))
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()
