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

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include "CheckSystem.h"
#include <utils.h>
#include <tlhelp32.h>
#include <EnumNtSrv.h>
#include <EnumProcess.h>
#include <checkHelpers.hpp>
#include <map>
#include <set>
#include <sysinfo.h>
#include <filter_framework.hpp>
#include <simple_registry.hpp>
#include <settings/client/settings_client.hpp>
#include <arrayBuffer.h>

#include <config.h>

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

namespace sh = nscapi::settings_helper;

/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckSystem::loadModule() {
	return loadModuleEx(_T(""), NSCAPI::normalStart);
}

/**
 * New version of the load call.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */

bool CheckSystem::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	PDHCollector::system_counter_data *data = new PDHCollector::system_counter_data;
	data->check_intervall = 100;
	try {
		typedef std::map<std::wstring,std::wstring> counter_map_type;
		std::map<std::wstring,std::wstring> counters;
		bool default_counters = true;

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("check"), alias, _T("system/windows"));

		settings.alias().add_path_to_settings()
			(_T("WINDOWS CHECK SYSTEM"), _T("Section for system checks and system settings"))

			(_T("service mapping"), _T("SERVICE MAPPING SECTION"), _T("Confiure which services has to be in which state"))

			(_T("pdh"), _T("PDH COUNTER INFORMATION"), _T(""))

			(_T("pdh/counters"), sh::wstring_map_path(&counters)
			, _T("PDH COUNTERS"), _T(""))


			;


 		settings.alias().add_key_to_settings()
			(_T("default"), sh::bool_key(&default_counters, true),
			_T("DEFAULT COUNTERS"), _T("Load the default counters: ") PDH_SYSTEM_KEY_CPU _T(", ") PDH_SYSTEM_KEY_MCB _T(", ") PDH_SYSTEM_KEY_MCL _T(" and ") PDH_SYSTEM_KEY_UPT _T(" If not you need to specify these manually. ") )

			(_T("default"), sh::bool_key(&default_counters, true),
			_T("DEFAULT COUNTERS"), _T("Load the default counters: ") PDH_SYSTEM_KEY_CPU _T(", ") PDH_SYSTEM_KEY_MCB _T(", ") PDH_SYSTEM_KEY_MCL _T(" and ") PDH_SYSTEM_KEY_UPT _T(" If not you need to specify these manually. ") )

			(_T("default buffer length"), sh::wstring_key(&data->buffer_length, _T("1h")),
			_T("DEFAULT INTERVALL"), _T("Used to define the default intervall for range buffer checks (ie. CPU)."))
// 
// 			(_T("hostname cache"), sh::bool_key(&cacheNscaHost_),
// 			_T("CACHE HOSTNAME"), _T(""))
// 
// 			(_T("delay"), sh::string_fun_key<std::wstring>(boost::bind(&NSCAAgent::set_delay, this, _1), 0),
// 			_T("DELAY"), _T(""))
// 
// 			(_T("payload length"), sh::uint_key(&payload_length_, 512),
// 			_T("PAYLOAD LENGTH"), _T("The password to use. Again has to be the same as the server or it wont work at all."))

			;

		//std::map<DWORD,std::wstring>::key_type

		settings.alias().add_key_to_settings()

			(_T("BOOT_START"), sh::wstring_vector_key(&lookups_, SERVICE_BOOT_START, _T("ignored")),
			_T("SERVICE_BOOT_START"), _T("TODO"))

			(_T("SYSTEM_START"), sh::wstring_vector_key(&lookups_, SERVICE_SYSTEM_START, _T("ignored")),
			_T("SERVICE_SYSTEM_START"), _T("TODO"))

			(_T("AUTO_START"), sh::wstring_vector_key(&lookups_, SERVICE_AUTO_START, _T("started")),
			_T("SERVICE_AUTO_START"), _T("TODO"))

			(_T("DEMAND_START"), sh::wstring_vector_key(&lookups_, SERVICE_DEMAND_START, _T("ignored")),
			_T("SERVICE_DEMAND_START"), _T("TODO"))

			(_T("DISABLED"), sh::wstring_vector_key(&lookups_, SERVICE_DISABLED, _T("stopped")),
			_T("SERVICE_DISABLED"), _T("TODO"))

			(_T("DELAYED"), sh::wstring_vector_key(&lookups_, NSCP_SERVICE_DELAYED, _T("ignored")),
			_T("SERVICE_DELAYED"), _T("TODO"))

			;

		settings.register_all();
		settings.notify();

		typedef PDHCollector::system_counter_data::counter cnt;
		if (default_counters) {
			data->counters.push_back(cnt(PDH_SYSTEM_KEY_CPU, _T("\\238(_total)\\6"), cnt::type_int64, cnt::format_large, cnt::rrd));
			data->counters.push_back(cnt(PDH_SYSTEM_KEY_MCB, _T("\\4\\26"), cnt::type_int64, cnt::format_large, cnt::value));
			data->counters.push_back(cnt(PDH_SYSTEM_KEY_MCL, _T("\\4\\30"), cnt::type_int64, cnt::format_large, cnt::value));
			data->counters.push_back(cnt(PDH_SYSTEM_KEY_UPT, _T("\\2\\674"), cnt::type_int64, cnt::format_large, cnt::value));
		}
		BOOST_FOREACH(counter_map_type::value_type c, counters) {
			data->counters.push_back(cnt(c.first, c.second, cnt::type_int64, cnt::format_large, cnt::value));
		}

		register_command(_T("checkCPU"), _T("Check the CPU load of the computer."));
		register_command(_T("checkUpTime"), _T("Check the up-time of the computer."));
		register_command(_T("checkServiceState"), _T("Check the state of one or more of the computer services."));
		register_command(_T("checkProcState"), _T("Check the state of one or more of the processes running on the computer."));
		register_command(_T("checkMem"), _T("Check free/used memory on the system."));
		register_command(_T("checkCounter"), _T("Check a PDH counter."));
		register_command(_T("listCounterInstances"), _T("List all instances for a counter."));
		register_command(_T("checkSingleRegEntry"), _T("Check registry key"));
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}

	if (mode == NSCAPI::normalStart) {
		pdhThread.createThread(data);
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

int CheckSystem::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	if (command == _T("help")) {
		std::wcerr << _T("Usage: ... CheckSystem <command>") << std::endl;
		std::wcerr << _T("Commands: debugpdh, listpdh, pdhlookup, pdhmatch, pdhobject") << std::endl;
		return -1;
	}
	if (command == _T("debugpdh")) {
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
						if (PDH::PDHResolver::validate(counter, error, false)) {
							status.push_back(_T("open"));
						} else {
							errors.push_back(_T("NOT found: ") + error);
							bStatus = false;
						}
						if (bStatus) {
							
							typedef boost::shared_ptr<PDH::PDHCounter> counter_ptr;
							counter_ptr pCounter;
							PDH::PDHQuery pdh;
							try {
								pdh.addCounter(counter);
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
				if ((*it).counters.size() == 0) {
					std::wcout << _T("empty counter: ") << (*it).name << std::endl;
				}
				for (PDH::Enumerations::Counters::const_iterator it2 = (*it).counters.begin();it2!=(*it).counters.end();++it2) {
					std::wstring counter = _T("\\") + (*it).name + _T("\\") + (*it2).name;
					std::wcout << _T("testing: ") << counter << _T(": ");
					std::wstring error;
					if (PDH::PDHResolver::validate(counter, error, false)) {
						std::wcout << _T(" found ");
					} else {
						std::wcout << _T(" *NOT* found (") << error << _T(") ") << std::endl;
						break;
					}
					bool bOpend = false;
					try {
						PDH::PDHQuery pdh;
						//PDHCollectors::StaticPDHCounterListener<double, PDH_FMT_DOUBLE> cDouble;
						pdh.addCounter(counter);
						pdh.open();
						pdh.gatherData();
						pdh.close();
						bOpend = true;
					} catch (const PDH::PDHException e) {
						std::wcout << _T(" could *not* be open (") << e.getError() << _T(") ") << std::endl;
						break;
					}
					std::wcout << _T(" open ");
					std::wcout << std::endl;
				}
			}
		}
	} else if (command == _T("listpdh")) {
		bool porcelain = arguments.size() > 0 && arguments.front() == _T("--porcelain");
		PDH::Enumerations::Objects lst;
		try {
			lst = PDH::Enumerations::EnumObjects();
		} catch (const PDH::PDHException e) {
			result = _T("ERROR: Service enumeration failed: ") + e.getError();
			return NSCAPI::returnUNKNOWN;
		}
		std::wstringstream ss;
		BOOST_FOREACH(PDH::Enumerations::Object &obj, lst) {
			if (!obj.error.empty()) {
				ss << "error," << obj.name << "," << utf8::to_unicode(obj.error) << _T("\n");
			} else if (obj.instances.size() > 0) {
				BOOST_FOREACH(const PDH::Enumerations::Instance &inst, obj.instances) {
					BOOST_FOREACH(const PDH::Enumerations::Counter &count, obj.counters) {
						if (porcelain) 
							ss << "counter," << obj.name << _T(",") << inst.name << _T(",") << count.name << _T("\n");
						else
							ss << _T("\\") << obj.name << _T("(") << inst.name << _T(")\\") << count.name << _T("\n");
					}
				}
			} else {
				BOOST_FOREACH(const PDH::Enumerations::Counter &count, obj.counters) {
					if (porcelain) 
						ss << obj.name << _T(",") << count.name << _T("\n");
					else
						ss << _T("\\") << obj.name << _T("\\") << count.name << _T("\n");
				}
			}
		}
		result = ss.str();
		return NSCAPI::returnOK;
	} else if (command == _T("pdhlookup")) {
		try {
			if (arguments.size() == 0) {
				NSC_LOG_ERROR_STD(_T("Need to specify counter index name!"));
				return 0;
			}
			std::wstring name = arguments.front();
			DWORD dw = PDH::PDHResolver::lookupIndex(name);
			NSC_LOG_MESSAGE_STD(_T("--+--[ Lookup Result ]----------------------------------------"));
			NSC_LOG_MESSAGE_STD(_T("  | Index for '") + name + _T("' is ") + strEx::itos(dw));
			NSC_LOG_MESSAGE_STD(_T("--+-----------------------------------------------------------"));
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("Failed to lookup index: ") + e.getError());
			return 0;
		}
	} else if (command == _T("pdhmatch")) {
		try {
			if (arguments.size() == 0) {
				NSC_LOG_ERROR_STD(_T("Need to specify counter index name!"));
				return 0;
			}
			std::wstring name = arguments.front();
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
	} else if (command == _T("pdhobject")) {
		try {
			if (arguments.size() == 0) {
				NSC_LOG_ERROR_STD(_T("Need to specify counter index name!"));
				return 0;
			}
			std::wstring name = arguments.front();
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
NSCAPI::nagiosReturn CheckSystem::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	CheckSystem::returnBundle rb;
	if (command == _T("checkcpu")) {
		return checkCPU(arguments, message, perf);
	} else if (command == _T("checkuptime")) {
		return checkUpTime(arguments, message, perf);
	} else if (command == _T("checkservicestate")) {
		return checkServiceState(arguments, message, perf);
	} else if (command == _T("checkprocstate")) {
		return checkProcState(arguments, message, perf);
	} else if (command == _T("checkmem")) {
		return checkMem(arguments, message, perf);
	} else if (command == _T("checkcounter")) {
		return checkCounter(arguments, message, perf);
	} else if (command == _T("listcounterinstances")) {
		return listCounterInstances(arguments, message, perf);
	} else if (command == _T("checksingleregentry")) {
		return checkSingleRegEntry(arguments, message, perf);
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
		return boost::lexical_cast<std::wstring>(value);
	}
	static std::wstring print_percent(int value) {
		return boost::lexical_cast<std::wstring>(value) + _T("%");
	}
	static std::wstring key_prefix() {
		return _T("average load ");
	}
	static std::wstring key_postfix() {
		return _T("");
	}
};
NSCAPI::nagiosReturn CheckSystem::checkCPU(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) {
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericBounds<int, cpuload_handler> > > CPULoadContainer;

	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<CPULoadContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	CPULoadContainer tmpObject;

	tmpObject.data = _T("cpuload");

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_NUMERIC_ALL(tmpObject, _T(""))
		MAP_OPTIONS_STR(_T("warn"), tmpObject.warn.max_)
		MAP_OPTIONS_STR(_T("crit"), tmpObject.crit.max_)
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
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}

NSCAPI::nagiosReturn CheckSystem::checkUpTime(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsTime> UpTimeContainer;

	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bNSClient = false;
	bool bPerfData = true;
	UpTimeContainer bounds;

	bounds.data = _T("uptime");

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_NUMERIC_ALL(bounds, _T(""))
		MAP_OPTIONS_STR(_T("warn"), bounds.warn.min_)
		MAP_OPTIONS_STR(_T("crit"), bounds.crit.min_)
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
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
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
NSCAPI::nagiosReturn CheckSystem::checkServiceState(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::SimpleBoundsStateBoundsInteger> StateContainer;
	if (arguments.empty()) {
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
	MAP_OPTIONS_BEGIN(arguments)
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

		bool vista = systemInfo::isAboveVista(systemInfo::getOSVersion());
		std::list<TNtServiceInfo> service_list_automatic = TNtServiceInfo::EnumServices(SERVICE_WIN32,SERVICE_INACTIVE|SERVICE_ACTIVE, vista); 
		for (std::list<TNtServiceInfo>::const_iterator service =service_list_automatic.begin();service!=service_list_automatic.end();++service) { 
			if (excludeList.find((*service).m_strServiceName) == excludeList.end()) {
				tmpObject.data = (*service).m_strServiceName;
				std::wstring x = lookups_[(*service).m_dwStartType];
				if (x != _T("ignored")) {
					tmpObject.crit.state = x;
					list.push_back(tmpObject); 
				}
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
				nscapi::plugin_helper::escalteReturnCodeToWARN(returnCode);
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
						nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
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
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": Whooha this is odd.");
	else if (!bNSClient)
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
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
NSCAPI::nagiosReturn CheckSystem::checkMem(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBounds<checkHolders::NumericPercentageBounds<checkHolders::PercentageValueType<unsigned __int64, unsigned __int64>, checkHolders::disk_size_handler<unsigned __int64> > > > MemoryContainer;
	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<MemoryContainer> list;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	bool bShowAll = false;
	bool bPerfData = true;
	bool bNSClient = false;
	MemoryContainer tmpObject;

	MAP_OPTIONS_BEGIN(arguments)
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
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}
typedef struct NSPROCDATA__ {
	unsigned int count;
	unsigned int hung_count;
	CEnumProcess::CProcessEntry entry;
	std::wstring key;

	NSPROCDATA__() : count(0), hung_count(0) {}
	NSPROCDATA__(const NSPROCDATA__ &other) : count(other.count), hung_count(other.hung_count), entry(other.entry), key(other.key) {}
} NSPROCDATA;
typedef std::map<std::wstring,NSPROCDATA> NSPROCLST;

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
			key = boost::to_lower_copy((*entry).filename);
		NSPROCLST::iterator it = ret.find(key);
		if (it == ret.end()) {
			ret[key].entry = (*entry);
			ret[key].count = 1;
			ret[key].hung_count = (*entry).hung?1:0;
			ret[key].key = key;
		} else {
			if ((*entry).hung) 
				(*it).second.hung_count++;
			(*it).second.count++;
		}
	}
	return ret;
}



struct process_count_result {
	unsigned long running;
	unsigned long hung;
	process_count_result() : running(0), hung(0) {}

	std::wstring format_value(std::wstring tag, unsigned long count) {
		if (count > 1)
			return tag + _T("(") + strEx::itos(count) +_T(")");
		if (count == 1)
			return tag;
		return _T("");
	}
	std::wstring to_wstring() {
		if (running > 0 && hung > 0)
			return format_value(_T("running"), running) + _T(", ") + format_value(_T("hung"), hung);
		if (running > 0)
			return format_value(_T("running"), running);
		if (hung > 0)
			return format_value(_T("hung"), hung);
		return _T("stopped");
	}
	std::wstring to_wstring_short() {
		if (running > 0 && hung > 0)
			return _T("running, hung");
		if (running > 0)
			return _T("running");
		if (hung > 0)
			return _T("hung");
		return _T("stopped");
	}

};

class ProcessBound {
public:
	checkHolders::ExactBounds<checkHolders::NumericBounds<unsigned long, checkHolders::int_handler> > running;
	checkHolders::ExactBounds<checkHolders::NumericBounds<unsigned long, checkHolders::int_handler> > hung;
	typedef checkHolders::NumericBounds<unsigned long, checkHolders::int_handler> THolder;

	typedef ProcessBound TMyType;
	typedef process_count_result TValueType;

	ProcessBound() {}
	ProcessBound(const ProcessBound &other) {
		running = other.running;
		hung = other.hung;
	}

	void reset() {
		running.reset();
		hung.reset();
	}
	bool hasBounds() {
		return running.hasBounds() || hung.hasBounds();
	}
	static std::wstring toStringLong(TValueType &value) {
		return value.to_wstring();
	}
	static std::wstring toStringShort(TValueType &value) {
		return value.to_wstring_short();
	}
	std::wstring gatherPerfData(std::wstring alias, TValueType &value, TMyType &warn, TMyType &crit) {
		if (hung.hasBounds())
			return hung.gatherPerfData(alias, value.hung, warn.hung, crit.hung);
		return running.gatherPerfData(alias, value.running, warn.running, crit.running);
	}
	std::wstring gatherPerfData(std::wstring alias, TValueType &value) {
		THolder tmp;
		if (hung.hasBounds())
			return tmp.gatherPerfData(alias, value.hung);
		return tmp.gatherPerfData(alias, value.running);
	}
	bool check(TValueType &value, std::wstring lable, std::wstring &message, checkHolders::ResultType type) {
		if (hung.hasBounds()) {
			return hung.check_preformatted(value.hung, value.to_wstring(), lable, message, type);
		} else {
			return running.check_preformatted(value.running, value.to_wstring(), lable, message, type);
		}
		return false;
	}

};
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
NSCAPI::nagiosReturn CheckSystem::checkProcState(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<ProcessBound> StateContainer;
	
//	typedef checkHolders::CheckContainer<checkHolders::ExactBoundsState> StateContainer2;

	if (arguments.empty()) {
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
	bool ignoreState = false;
	typedef enum {
		match_string, match_substring, match_regexp
	} match_type;
	match_type match = match_string;

	

	//tmpObject.data = _T("uptime");
	//tmpObject.crit.min = 1;

	MAP_OPTIONS_BEGIN(arguments)
		//MAP_OPTIONS_NUMERIC_ALL(tmpObject, _T("Count"))
		MAP_OPTIONS_EXACT_NUMERIC_ALL_EX(tmpObject, _T("Count"), running)
		MAP_OPTIONS_EXACT_NUMERIC_LEGACY_EX(tmpObject, _T("Count"), running)
		MAP_OPTIONS_EXACT_NUMERIC_ALL_EX(tmpObject, _T("HungCount"), hung)
		MAP_OPTIONS_EXACT_NUMERIC_LEGACY_EX(tmpObject, _T("HungCount"), hung)
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.alias)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE(_T("ignore-state"), ignoreState)
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
			if (p__.second.empty()) {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.min = _T("1"); 
			} else if (p__.second == _T("started")) {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.min = _T("1");
			} else if (p__.second == _T("stopped")) {
				if (!tmpObject.crit.running.hasBounds() && !tmpObject.warn.running.hasBounds())
					tmpObject.crit.running.max = _T("0");
			} else if (p__.second == _T("hung")) {
				if (!tmpObject.crit.hung.hasBounds() && !tmpObject.warn.hung.hasBounds())
					tmpObject.crit.hung.max = _T("1");
			}
			list.push_back(tmpObject);
			tmpObject.reset();
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
				nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
			}
		} else {
			process_count_result value;
			if (bFound) {
				value.hung = (*proc).second.hung_count;
				value.running = (*proc).second.count;
			} else {
				value.hung = 0;
				value.running = 0;
//				if (ignoreState)
//					value.state = checkHolders::state_stopped | checkHolders::state_started | checkHolders::state_hung;
//				else
//				value.state = checkHolders::state_stopped;
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
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
	return returnCode;
}

template<class T>
class PerfDataContainer : public checkHolders::CheckContainer<T> {
private:
	typedef PDHCollectors::StaticPDHCounterListener<double, PDH_FMT_DOUBLE> counter_type;
	typedef boost::shared_ptr<counter_type> ptr_lsnr_type;
	ptr_lsnr_type cDouble;
public:

	PerfDataContainer() : CheckContainer<T>() {}

	PerfDataContainer(const PerfDataContainer &other) : CheckContainer<T>(other), cDouble(other.cDouble) {}
	const PerfDataContainer& operator =(const PerfDataContainer &other) {
		*((CheckContainer<T>*)this) = other;
		cDouble = other.cDouble;
		return *this;
	}
	ptr_lsnr_type get_listener() {
		if (!cDouble)
			cDouble = ptr_lsnr_type(new counter_type());
		return cDouble;
	}
};

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
NSCAPI::nagiosReturn CheckSystem::checkCounter(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf)
{
	typedef PerfDataContainer<checkHolders::MaxMinBoundsDouble> CounterContainer;

	if (arguments.empty()) {
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
	bool bExpandIndex = false;
	bool bForceReload = false;
	std::wstring extra_format;

	MAP_OPTIONS_BEGIN(arguments)
		MAP_OPTIONS_STR(_T("InvalidStatus"), invalidStatus)
		MAP_OPTIONS_STR_AND(_T("Counter"), tmpObject.data, counters.push_back(tmpObject))
		MAP_OPTIONS_STR(_T("MaxWarn"), tmpObject.warn.max_)
		MAP_OPTIONS_STR(_T("MinWarn"), tmpObject.warn.min_)
		MAP_OPTIONS_STR(_T("MaxCrit"), tmpObject.crit.max_)
		MAP_OPTIONS_STR(_T("MinCrit"), tmpObject.crit.min_)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_STR(_T("Alias"), tmpObject.data)
		MAP_OPTIONS_STR(_T("format"), extra_format)
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_EX(_T("Averages"), bCheckAverages, _T("true"), _T("false"))
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		MAP_OPTIONS_BOOL_TRUE(_T("index"), bExpandIndex)
		MAP_OPTIONS_BOOL_TRUE(_T("reload"), bForceReload)
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

	if (counters.empty()) {
		msg = _T("No counters specified");
		return NSCAPI::returnUNKNOWN;
	}
	PDH::PDHQuery pdh;

	bool has_counter = false;
	BOOST_FOREACH(CounterContainer &counter, counters) {
		try {
			if (counter.data.find('\\') == std::wstring::npos) {
				/*
				value = pObject->get_double(counter.data);
				if (value == -1) {
					msg = _T("ERROR: Failed to get counter value: ") + counter.data;
					return NSCAPI::returnUNKNOWN;
				}
				*/
			} else {
				std::wstring tstr;
				if (bExpandIndex) {
					PDH::PDHResolver::expand_index(counter.data);
				}
				if (!PDH::PDHResolver::validate(counter.data, tstr, bForceReload)) {
					NSC_LOG_ERROR_STD(_T("ERROR: Counter not found: ") + counter.data + _T(": ") + tstr);
					if (bNSClient) {
						NSC_LOG_ERROR_STD(_T("ERROR: Counter not found: ") + counter.data + _T(": ") + tstr);
						//msg = _T("0");
					} else {
						msg = _T("CRIT: Counter not found: ") + counter.data + _T(": ") + tstr;
						return NSCAPI::returnCRIT;
					}
				}
				if (!extra_format.empty()) {
					boost::char_separator<wchar_t> sep(_T(","));

					boost::tokenizer< boost::char_separator<wchar_t>, std::wstring::const_iterator, std::wstring > tokens(extra_format, sep);
					DWORD flags = 0;
					BOOST_FOREACH(std::wstring t, tokens) {
						if (extra_format == _T("nocap100"))
							flags |= PDH_FMT_NOCAP100;
						else if (extra_format == _T("1000"))
							flags |= PDH_FMT_1000;
						else if (extra_format == _T("noscale"))
							flags |= PDH_FMT_NOSCALE;
						else {
							NSC_LOG_ERROR_STD(_T("Unsupported extrta format: ") + extra_format);
						}
					}
					counter.get_listener()->set_extra_format(flags);
				}
				pdh.addCounter(counter.data, counter.get_listener());
				has_counter = true;
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
	if (has_counter) {
		try {
			pdh.open();
			if (bCheckAverages) {
				pdh.collect();
				Sleep(1000);
			}
			pdh.collect();
			pdh.gatherData();
			pdh.close();
		} catch (const PDH::PDHException e) {
			NSC_LOG_ERROR_STD(_T("ERROR: ") + e.getError());
			if (bNSClient)
				msg = _T("0");
			else
				msg = _T("ERROR: ") + e.getError();
			return NSCAPI::returnUNKNOWN;
		}
	}
	BOOST_FOREACH(CounterContainer &counter, counters) {
		try {
			double value = 0;
			if (counter.data.find('\\') == std::wstring::npos) {
				PDHCollector *pObject = pdhThread.getThread();
				if (!pObject) {
					msg = _T("ERROR: PDH Collection thread not running.");
					return NSCAPI::returnUNKNOWN;
				}
				value = pObject->get_double(counter.data);
				if (value == -1) {
					msg = _T("ERROR: Failed to get counter value: ") + counter.data;
					return NSCAPI::returnUNKNOWN;
				}
			} else {
				value = counter.get_listener()->getValue();
			}

			if (bNSClient) {
				if (!msg.empty())
					msg += _T(",");
				msg += strEx::itos(static_cast<float>(value));
			} else {
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
		msg = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + msg;
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
NSCAPI::nagiosReturn CheckSystem::listCounterInstances(std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf)
{
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDouble> CounterContainer;

	if (arguments.empty()) {
		msg = _T("ERROR: Missing argument exception.");
		return NSCAPI::returnUNKNOWN;
	}

	std::wstring counter;
	BOOST_FOREACH(std::wstring s, arguments) { counter+= s + _T(" "); }
	try {
		PDH::Enumerations::pdh_object_details obj = PDH::Enumerations::EnumObjectInstances(counter);
		for (PDH::Enumerations::pdh_object_details::list::const_iterator it = obj.instances.begin(); it!=obj.instances.end();++it) {
			if (!msg.empty())
				msg += _T(", ");
			msg += (*it);
		}
		if (msg.empty()) {
			msg = _T("ERROR: No instances found");
			return NSCAPI::returnUNKNOWN;
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct regkey_info {

	std::wstring error;

	static regkey_info get(__int64 now, std::wstring path) {
		return regkey_info(now, path);
	}

	regkey_info() 
		: ullLastWriteTime(0)
		, iType(0)
		, ullNow(0)
		, uiExists(0)
		, ullChildCount(0)
	{}
	regkey_info(__int64 now, std::wstring path) 
		: path(path)
		, ullLastWriteTime(0)
		, iType(0)
		, ullNow(now)
		, uiExists(0)
		, ullChildCount(0)
	{
		std::wstring key;
		try {
			std::wcout << _T("opening: ") << path << std::endl;
			std::wstring::size_type pos = path.find_first_of(L'\\');
			if (pos != std::wstring::npos)  {
				key = path.substr(0, pos);
				path = path.substr(pos+1);
				std::wcout << key << _T(":") << path << std::endl;
				simple_registry::registry_key rkey(simple_registry::parseHKEY(key), path);
				info = rkey.get_info();
				uiExists = 1;
			} else {
				error = _T("Failed to parse key");
			}
		} catch (simple_registry::registry_exception &e) {
			try {
				std::wstring::size_type pos = path.find_last_of(L'\\');
				if (pos != std::wstring::npos)  {
					std::wstring item = path.substr(pos+1);
					path = path.substr(0, pos);
					std::wcout << key << _T(":") << path << _T(".") << item << std::endl;
					simple_registry::registry_key rkey(simple_registry::parseHKEY(key), path);
					info = rkey.get_info(item);
					uiExists = 1;
				} else {
					error = _T("Failed to parse key");
				}
			} catch (simple_registry::registry_exception &e) {
				//error = e.what();
			}
		} catch (...) {
			error = _T("Unknown exception");
		}

		//HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\services\eventlog\Application\MaxSize

		// TODO get key info here!
		//ullLastWriteTime = ((info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime);
	};

	unsigned long long ullSize;
	__int64 ullLastWriteTime;
	__int64 ullNow;
	std::wstring filename;
	std::wstring path;
	unsigned long ullChildCount;
	unsigned long uiExists;
	unsigned int iType;
	simple_registry::registry_key::reg_info info;

	static const __int64 MSECS_TO_100NS = 10000;

	__int64 get_written() {
		return (ullNow-ullLastWriteTime)/MSECS_TO_100NS;
	}
	std::wstring render(std::wstring syntax) {
		strEx::replace(syntax, _T("%path%"), path);
		strEx::replace(syntax, _T("%key%"), filename);
		strEx::replace(syntax, _T("%write%"), strEx::format_filetime(ullLastWriteTime, DATE_FORMAT));
		strEx::replace(syntax, _T("%write-raw%"), strEx::itos(ullLastWriteTime));
		strEx::replace(syntax, _T("%now-raw%"), strEx::itos(ullNow));
		strEx::replace(syntax, _T("%type%"), strEx::itos_as_BKMG(iType));
		strEx::replace(syntax, _T("%child-count%"), strEx::itos(ullChildCount));
		strEx::replace(syntax, _T("%exists%"), strEx::itos(uiExists));
		strEx::replace(syntax, _T("%int%"), strEx::itos(info.iValue));
		strEx::replace(syntax, _T("%int-value%"), strEx::itos(info.iValue));
		strEx::replace(syntax, _T("%string%"), info.sValue);
		strEx::replace(syntax, _T("%string-value%"), info.sValue);
		return syntax;
	}
};


struct regkey_filter {
	filters::filter_all_times written;
	filters::filter_all_num_ul type;
	filters::filter_all_num_ul exists;
	filters::filter_all_num_ul child_count;
	filters::filter_all_num_ll value_int;
	filters::filter_all_strings value_string;

	inline bool hasFilter() {
		return type.hasFilter() || exists.hasFilter() || written.hasFilter() || child_count.hasFilter() || value_int.hasFilter() || value_string.hasFilter();
	}
	bool matchFilter(regkey_info &value) const {
		if ((written.hasFilter())&&(written.matchFilter(value.get_written())))
			return true;
		else if (type.hasFilter()&&type.matchFilter(value.iType))
			return true;
		else if (exists.hasFilter()&&exists.matchFilter(value.uiExists))
			return true;
		else if ((child_count.hasFilter())&&(child_count.matchFilter(value.ullChildCount)))
			return true;
		else if ((value_int.hasFilter())&&(value_int.matchFilter(value.info.iValue)))
			return true;
		else if ((value_string.hasFilter())&&(value_string.matchFilter(value.info.sValue)))
			return true;
		return false;
	}

	std::wstring getValue() const {
		if (written.hasFilter())
			return _T("written: ") + written.getValue();
		if (type.hasFilter())
			return _T("type: ") + type.getValue();
		if (exists.hasFilter())
			return _T("exists: ") + exists.getValue();
		if (child_count.hasFilter())
			return _T("child_count: ") + child_count.getValue();
		if (value_int.hasFilter())
			return _T("value(i): ") + value_int.getValue();
		if (value_string.hasFilter())
			return _T("value(s): ") + value_string.getValue();
		return _T("UNknown...");
	}

};


struct regkey_container : public regkey_info {

	static regkey_container get(std::wstring path, unsigned long long now) {
		return regkey_container(now, path);
	}


	regkey_container(__int64 now, std::wstring path) : regkey_info(now, path) {}

	bool has_errors() {
		return !error.empty();
	}
	std::wstring get_error() {
		return error;
	}

};


class regkey_type_handler {
public:
	static int parse(std::wstring s) {
		return 1;
	}
	static std::wstring print(int value) {
		return _T("unknown");
	}
	static std::wstring print_unformated(int value) {
		return strEx::itos(value);
	}
	static std::wstring key_prefix() {
		return _T("");
	}
	static std::wstring key_postfix() {
		return _T("");
	}
	static std::wstring get_perf_unit(int  value) {
		return _T("");
	}
	static std::wstring print_perf(int  value, std::wstring unit) {
		return strEx::itos(value);
	}
};
class regkey_exists_handler {
public:
	static int parse(std::wstring s) {
		if (s  == _T("true"))
			return 1;
		return 0;
	}
	static std::wstring print(int value) {
		return value==1?_T("true"):_T("false");
	}
	static std::wstring print_unformated(int value) {
		return strEx::itos(value);
	}
	static std::wstring key_prefix() {
		return _T("");
	}
	static std::wstring key_postfix() {
		return _T("");
	}
	static std::wstring get_perf_unit(int value) {
		return _T("");
	}
	static std::wstring print_perf(int  value, std::wstring unit) {
		return strEx::itos(value);
	}
};

typedef checkHolders::CheckContainer<checkHolders::ExactBounds<checkHolders::NumericBounds<int, regkey_type_handler> > > RegTypeContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBounds<checkHolders::NumericBounds<int, regkey_exists_handler> > > RegExistsContainer;

typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULong> ExactULongContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsLongLong> ExactLongLongContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsTime> DateTimeContainer;
typedef checkHolders::CheckContainer<checkHolders::FilterBounds<filters::filter_all_strings> > StringContainer;

struct check_regkey_child_count : public checkHolders::check_proxy_container<regkey_container, ExactULongContainer> {
	check_regkey_child_count() { set_alias(_T("child-count")); }
	unsigned long get_value(regkey_container &value) {
		return value.ullChildCount;
	}
};
struct check_regkey_int_value : public checkHolders::check_proxy_container<regkey_container, ExactLongLongContainer> {
	check_regkey_int_value() { set_alias(_T("value")); }
	long long get_value(regkey_container &value) {
		return value.info.iValue;
	}
};
struct check_regkey_string_value : public checkHolders::check_proxy_container<regkey_container, StringContainer> {
	check_regkey_string_value() { set_alias(_T("value")); }
	std::wstring get_value(regkey_container &value) {
		return value.info.sValue;
	}
};
struct check_regkey_written : public checkHolders::check_proxy_container<regkey_container, DateTimeContainer> {
	check_regkey_written() { set_alias(_T("written")); }
	unsigned long long get_value(regkey_container &value) {
		return value.ullLastWriteTime;
	}
};
struct check_regkey_type : public checkHolders::check_proxy_container<regkey_container, RegTypeContainer> {
	check_regkey_type() { set_alias(_T("type")); }
	int get_value(regkey_container &value) {
		return value.iType;
	}
};
struct check_regkey_exists : public checkHolders::check_proxy_container<regkey_container, RegExistsContainer> {
	check_regkey_exists() { set_alias(_T("exists")); }
	int get_value(regkey_container &value) {
		return value.uiExists;
	}
};


typedef checkHolders::check_multi_container<regkey_container> check_file_multi;
struct check_regkey_factories {
	static checkHolders::check_proxy_interface<regkey_container>* type() {
		return new check_regkey_type();
	}
	static checkHolders::check_proxy_interface<regkey_container>* exists() {
		return new check_regkey_exists();
	}
	static checkHolders::check_proxy_interface<regkey_container>* child_count() {
		return new check_regkey_child_count();
	}
	static checkHolders::check_proxy_interface<regkey_container>* written() {
		return new check_regkey_written();
	}
	static checkHolders::check_proxy_interface<regkey_container>* value_string() {
		return new check_regkey_string_value();
	}
	static checkHolders::check_proxy_interface<regkey_container>* value_int() {
		return new check_regkey_int_value();
	}
};

#define MAP_FACTORY_PB(value, obj) \
		else if ((p__.first == _T("check")) && (p__.second == ##value)) { checker.add_check(check_regkey_factories::obj()); }


NSCAPI::nagiosReturn CheckSystem::checkSingleRegEntry(std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	check_file_multi checker;
	typedef std::pair<int,regkey_filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	if (arguments.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<std::wstring> files;
	unsigned int truncate = 0;
	std::wstring syntax = _T("%filename%");
	std::wstring alias;
	bool bPerfData = true;

	try {
		MAP_OPTIONS_BEGIN(arguments)
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), syntax)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_PUSH(_T("path"), files)
			MAP_OPTIONS_SHOWALL(checker)
			MAP_OPTIONS_EXACT_NUMERIC_ALL_MULTI(checker, _T(""))
			MAP_FACTORY_PB(_T("type"), type)
			MAP_FACTORY_PB(_T("child-count"), child_count)
			MAP_FACTORY_PB(_T("written"), written)
			MAP_FACTORY_PB(_T("int"), value_int)
			MAP_FACTORY_PB(_T("string"), value_string)
			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	}
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	unsigned __int64 nowi64 = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	for (std::list<std::wstring>::const_iterator pit = files.begin(); pit != files.end(); ++pit) {
		regkey_container info = regkey_container::get(*pit, nowi64);
		if (info.has_errors()) {
			message = info.error;
			return NSCAPI::returnUNKNOWN;
		}
		checker.alias = info.render(syntax);
		checker.runCheck(info, returnCode, message, perf);
	}
	if ((truncate > 0) && (message.length() > (truncate-4))) {
		message = message.substr(0, truncate-4) + _T("...");
		perf = _T("");
	}
	if (message.empty())
		message = _T("CheckSingleRegkey ok");
	return returnCode;
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(CheckSystem);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
