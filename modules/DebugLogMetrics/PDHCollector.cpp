//////////////////////////////////////////////////////////////////////////
// PDH Collector 
// 
// Functions from this file collects data from the PDH subsystem and stores 
// it for later use
// *NOTICE* that this is done in a separate thread so threading issues has 
// to be handled. I handle threading issues in the CounterListener's get/
// set accessors.
//
// Copyright (c) 2004 MySolutions NORDIC (http://www.medin.nu)
//
// Date: 2004-03-13
// Author: Michael Medin - <michael@medin.name>
//
// This software is provided "AS IS", without a warranty of any kind.
// You are free to use/modify this code but leave this header intact.
//
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PDHCollector.h"
#include <Settings.h>
#include <sysinfo.h>


PDHCollector::PDHCollector() : hStopEvent_(NULL) {
}

PDHCollector::~PDHCollector() 
{
	if (hStopEvent_)
		CloseHandle(hStopEvent_);
}

bool PDHCollector::loadCounter(PDH::PDHQuery &pdh) {
	try {
		OSVERSIONINFO osVer = systemInfo::getOSVersion();
		if (!systemInfo::isNTBased(osVer)) {
			NSC_LOG_ERROR_STD(_T("Detected Windows 3.x or Windows 9x, PDH will be disabled."));
			NSC_LOG_ERROR_STD(_T("To manual set performance counters you need to first set ") C_SYSTEM_AUTODETECT_PDH _T("=0 in the config file, and then you also need to configure the various counter."));
			return false;
		}
		if (systemInfo::isBelowNT4(osVer)) {
			NSC_DEBUG_MSG_STD(_T("Autodetected NT4 doing nothing, but still wasting reasources!"));
			return false;
		}
	} catch (const systemInfo::SystemInfoException &e) {
		NSC_LOG_ERROR_STD(_T("To manual set performance counters you need to first set ") C_SYSTEM_AUTODETECT_PDH _T("=0 in the config file, and then you also need to configure the various counter."));
		NSC_LOG_ERROR_STD(_T("The Error: ") + e.getError());
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("To manual set performance counters you need to first set ") C_SYSTEM_AUTODETECT_PDH _T("=0 in the config file, and then you also need to configure the various counter."));
		NSC_LOG_ERROR_STD(_T("The Error: UNKNOWN_EXCEPTION"));
		return false;
	}

	// Open counters via .defs file or index.
	try {
		std::wstring process_counter_name = pdh.lookupIndex(230);
		PDH::Enumerations::pdh_object_details list = PDH::Enumerations::EnumObjectInstances(process_counter_name);
		int count = 0;
		for (PDH::Enumerations::pdh_object_details::list::const_iterator cit = list.instances.begin(); cit != list.instances.end(); ++cit) {
			if (strEx::StrCmpI<std::wstring>(*cit,process_name_) == 0) {
				count++;
			}
		}
		PDHCollectors::StaticPDHCounterListener<unsigned int, PDHCollectors::format_long, PDHCollectors::PDHCounterNormalMutex> *pid_list = 
			new PDHCollectors::StaticPDHCounterListener<unsigned int, PDHCollectors::format_long, PDHCollectors::PDHCounterNormalMutex>[count];
		std::wstring name = _T("\\") + process_counter_name + _T("(") + process_name_ + _T(")\\") + pdh.lookupIndex(784);
		NSC_DEBUG_MSG_STD(_T("Adding counter: ") + name);
		pdh.addCounter(name, &pid_list[0]);
		for (int i=1;i<count;i++) {
			name = _T("\\") + process_counter_name + _T("(") + process_name_ + _T("#") + strEx::itos(i) + _T(")\\") + pdh.lookupIndex(784);
			NSC_DEBUG_MSG_STD(_T("Adding counter: ") + name);
			pdh.addCounter(name, &pid_list[i]);
		}
		pdh.open();
		try {
			pdh.gatherData();
		} catch (PDH::PDHException &e) {
			NSC_LOG_ERROR_STD(_T("Failed to get PID: ") + e.getError());
		}
		DWORD my_pid = GetCurrentProcessId();
		NSC_DEBUG_MSG_STD(_T("My PID is:") + strEx::itos(my_pid));
		int my_instance = 0;
		for (int i=0;i<count;i++) {
			DWORD other_pid = pid_list[i].getValue();
			if (my_pid == other_pid)
				my_instance = i;
		}
		pdh.removeAllCounters();
		std::wstring my_process_name = process_name_;
		if (my_instance > 0)
			my_process_name += _T("#") + strEx::itos(my_instance);

		list_.push_back(new pdh_value(784));
		list_.push_back(new pdh_value(952));
		list_.push_back(new pdh_value(680));
		list_.push_back(new pdh_value(784));
		list_.push_back(new pdh_value(172));
		list_.push_back(new pdh_value(174));
		list_.push_back(new pdh_value(178));
		list_.push_back(new pdh_value(180));
		list_.push_back(new pdh_value(172));
		list_.push_back(new pdh_value(174));
		list_.push_back(new pdh_value(182));
		list_.push_back(new pdh_value(184));

		std::wstring base_name = _T("\\") + pdh.lookupIndex(230) + _T("(") + my_process_name + _T(")\\");
		for (pdh_list::iterator it = list_.begin(); it != list_.end(); ++it) {
			(*it)->key = pdh.lookupIndex((*it)->index);
			std::wstring name = base_name + (*it)->key;
			NSC_DEBUG_MSG_STD(_T("Adding counter: ") + name);
			pdh.addCounter(name, &(*it)->value);
		}
		pdh.open();
	} catch (const PDH::PDHException &e) {
		NSC_LOG_ERROR_STD(_T("Failed to open performance counters: ") + e.getError());
		NSC_LOG_ERROR_STD(_T("Failed to open performance counters: ") + e.getError());
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to open performance counters: <Unhandled Exception>"));
		return false;
	}
	return true;
}

/**
* Thread that collects the data every "CHECK_INTERVAL" seconds.
*
* @param lpParameter Not used
* @return thread exit status
*
* @author mickem
*
* @date 03-13-2004               
*
* @bug If we have "custom named" counters ?
* @bug This whole concept needs work I think.
*
*/
DWORD PDHCollector::threadProc(LPVOID lpParameter) {
	try {
		//process_name_ = (reinterpret_cast<std::wstring*>(lpParameter))->c_str();
		process_name_ = _T("NSClient++");
		separator_ = _T("\t");
		event_ = _T("start");
		hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
		hStoreEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!hStopEvent_) {
			NSC_LOG_ERROR_STD(_T("Create StopEvent failed: ") + error::lookup::last_error());
			return 0;
		}
		file_.set_file(_T("nsclient++"),_T("process_info.csv"));
		PDH::PDHQuery pdh;
		bool bInit = true;
		{
			WriteLock lock(&mutex_, true, 5000);
			if (!lock.IsLocked()) {
				NSC_LOG_ERROR_STD(_T("Failed to get mutex when trying to start thread... thread will now die..."));
				bInit = false;
			} else if (!loadCounter(pdh)) {
				pdh.removeAllCounters();
				NSC_DEBUG_MSG_STD(_T("We aparently failed to load counters so we will waste reasources doing nothgin..."));
				bInit = false;
			}
		}

		std::wstring line = _T("Command");
		for (pdh_list::iterator it = list_.begin(); it != list_.end(); ++it) {
			line += separator_ + _T("\"") + (*it)->key + _T("\"");
		}
		file_.writeEntry(line + _T("\n"));


		HANDLE handles[] = {hStoreEvent_, hStopEvent_};
		DWORD waitStatus = 0;
		if (bInit) {
			bool first = true;
			do {
				std::list<std::wstring>	errors;
				std::wstring str;
				{
					ReadLock lock(&mutex_, true, 5000);
					if (!lock.IsLocked()) 
						NSC_LOG_ERROR(_T("Failed to get Mutex!"));
					else {
						try {
							pdh.gatherData();
							std::wstring line;
							for (pdh_list::iterator it = list_.begin(); it != list_.end(); ++it) {
								if (!line.empty())
									line += separator_;
								line += strEx::itos((*it)->value.getValue());
							}
							file_.writeEntry(event_ + separator_+  line + _T("\n"));
						} catch (PDH::PDHException &e) {
							errors.push_back(_T("Failed to query performance counters: ") + e.getError());
						} catch (...) {
							errors.push_back(_T("Failed to query performance counters: <Unhandled Exception>"));
						}
					} 
				}
				for (std::list<std::wstring>::const_iterator cit = errors.begin(); cit != errors.end(); ++cit) {
					NSC_LOG_ERROR_STD(*cit);
				}
				waitStatus = WaitForMultipleObjects(sizeof(handles)/sizeof(HANDLE), handles, FALSE, INFINITE);
			} while (waitStatus == WAIT_OBJECT_0);
		} else {
			NSC_LOG_ERROR_STD(_T("No performance counters were found we will not wait for the end instead..."));
			waitStatus = WaitForSingleObject(hStopEvent_, INFINITE);
		}
		if (waitStatus != WAIT_OBJECT_0) {
			NSC_LOG_ERROR(_T("Something odd happened when terminating PDH collection thread!"));
		}
		{
			WriteLock lock(&mutex_, true, 5000);
			if (!lock.IsLocked()) {
				NSC_LOG_ERROR(_T("Failed to get Mute when closing thread!"));
			}
			if (!CloseHandle(hStopEvent_)) {
				NSC_LOG_ERROR_STD(_T("Failed to close stopEvent handle: ") + error::lookup::last_error());
			} else
				hStopEvent_ = NULL;
			try {
				pdh.close();
			} catch (const PDH::PDHException &e) {
				NSC_LOG_ERROR_STD(_T("Failed to close performance counters: ") + e.getError());
			}
			for (pdh_list::iterator it = list_.begin(); it != list_.end(); ++it) {
				delete (*it);
			}
			list_.clear();
		}
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Unhandled exception in collection thread for the debug module: ") + strEx::string_to_wstring(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unhandled exception in collection thread for the debug module!"));
	}
	return 0;
}

void PDHCollector::store(std::wstring event) {
	{
		WriteLock lock(&mutex_, true, 5000);
		if (!lock.IsLocked()) {
			NSC_LOG_ERROR(_T("Failed to get Mute when writing datat (no data written)!"));
			return;
		}
		event_ = event;
	}
	if (!SetEvent(hStoreEvent_)) {
		NSC_LOG_ERROR_STD(_T("SetStopEvent failed"));
	}
}


/**
* Request termination of the thread (waiting for thread termination is not handled)
*/
void PDHCollector::exitThread(void) {
	if (hStopEvent_ == NULL)
		NSC_LOG_ERROR(_T("Stop event is not created!"));
	else if (!SetEvent(hStopEvent_)) {
			NSC_LOG_ERROR_STD(_T("SetStopEvent failed"));
	}
}

