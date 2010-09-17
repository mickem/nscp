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
#include <sysinfo.h>


PDHCollector::PDHCollector() : hStopEvent_(NULL), data_(NULL) {
	std::wstring subsystem = SETTINGS_GET_STRING(check_system::PDH_SUBSYSTEM);
	if (subsystem == setting_keys::check_system::PDH_SUBSYSTEM_FAST) {
	} else if (subsystem == setting_keys::check_system::PDH_SUBSYSTEM_THREAD_SAFE) {
		PDH::PDHFactory::set_threadSafe();
	} else {
		NSC_LOG_ERROR_STD(_T("Unknown PDH subsystem (") + subsystem + _T(") valid values are: fast and thread-safe"));
	}
}

PDHCollector::~PDHCollector()
{
	if (hStopEvent_)
		CloseHandle(hStopEvent_);
	delete data_;
}

boost::shared_ptr<PDHCollectors::PDHCollector> PDHCollector::system_counter_data::counter::get_counter(int check_intervall) {
	if (data_type == type_uint64 && data_format == format_large && collection_strategy == value) {
		return boost::shared_ptr<PDHCollectors::PDHCollector>(new PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex>);
	} else if (data_type == type_int64 && data_format == format_large && collection_strategy == value) {
		return boost::shared_ptr<PDHCollectors::PDHCollector>(new PDHCollectors::StaticPDHCounterListener<__int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex>);
	} else if (data_type == type_int64 && data_format == format_large && collection_strategy == rrd) {
		return boost::shared_ptr<PDHCollectors::PDHCollector>(new PDHCollectors::RoundINTPDHBufferListener<__int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex>(get_length(check_intervall)));
	}
	return boost::shared_ptr<PDHCollectors::PDHCollector>();
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
	hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent_) {
		NSC_LOG_ERROR_STD(_T("Create StopEvent failed: ") + error::lookup::last_error());
		return 0;
	}

	data_ = reinterpret_cast<system_counter_data*>(lpParameter);

	PDH::PDHQuery pdh;
	bool bInit = true;

	{
		WriteLock lock(&mutex_, true, 5000);
		if (!lock.IsLocked()) {
			NSC_LOG_ERROR_STD(_T("Failed to get mutex when trying to start thread... thread will now die..."));
			bInit = false;
		} else {
			pdh.removeAllCounters();
			NSC_DEBUG_MSG_STD(_T("Loading counters..."));
			BOOST_FOREACH(system_counter_data::counter c, data_->counters) {
				NSC_DEBUG_MSG_STD(_T("Loading counter: ") + c.alias + _T(" = ") + c.path);
				collector_ptr collector = c.get_counter(data_->check_intervall);
				if (collector) {
					counters_[c.alias] = collector;
					pdh.addCounter(c.path, collector);
				} else {
					NSC_LOG_ERROR_STD(_T("Failed to load counter: ") + c.alias + _T(" = ") + c.path);
				}
			}
			//SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),SORT_DEFAULT));
// 			pdh.addCounter(SETTINGS_GET_STRING(check_system::PDH_MEM_CMT_LIM), &memCmtLim);
// 			pdh.addCounter(SETTINGS_GET_STRING(check_system::PDH_MEM_CMT_BYT), &memCmt);
// 			pdh.addCounter(SETTINGS_GET_STRING(check_system::PDH_SYSUP), &upTime);
// 			pdh.addCounter(SETTINGS_GET_STRING(check_system::PDH_CPU), &cpu);
			try {
				pdh.open();
			} catch (const PDH::PDHException &e) {
				NSC_LOG_ERROR_STD(_T("Failed to open performance counters: ") + e.getError());
				bInit = false;
			}
		}
	}

	DWORD waitStatus = 0;
	if (bInit) {
		bool first = true;
		do {
			std::list<std::wstring>	errors;
			{
				ReadLock lock(&mutex_, true, 5000);
				if (!lock.IsLocked()) 
					NSC_LOG_ERROR(_T("Failed to get Mutex!"));
				else {
					try {
						pdh.gatherData();
					} catch (const PDH::PDHException &e) {
						if (first) {	// If this is the first run an error will be thrown since the data is not yet available
							// This is "ok" but perhaps another solution would be better, but this works :)
							first = false;
						} else {
							errors.push_back(_T("Failed to query performance counters: ") + e.getError());
						}
					} catch (...) {
						errors.push_back(_T("Failed to query performance counters: "));
					}
				} 
			}
			for (std::list<std::wstring>::const_iterator cit = errors.begin(); cit != errors.end(); ++cit) {
				NSC_LOG_ERROR_STD(*cit);
			}
		} while (((waitStatus = WaitForSingleObject(hStopEvent_, data_->check_intervall*100)) == WAIT_TIMEOUT));
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
	}
	return 0;
}

__int64 PDHCollector::get_int_value(std::wstring counter) {
	ReadLock lock(&mutex_, true, 5000);
	if (!lock.IsLocked())  {
		NSC_LOG_ERROR(_T("Failed to get Mutex for: ") + counter);
		return 0;
	}

	counter_map::iterator it = counters_.find(counter);
	if (it == counters_.end())
		return 0;
	collector_ptr ptr = (*it).second;
	return ptr->get_int64();
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
/**
* Get the average CPU usage for "time"
* @param time Time to check 
* @return average CPU usage
*/
int PDHCollector::getCPUAvrage(std::wstring time) {
	unsigned int mseconds = strEx::stoui_as_time(time, 100);
	ReadLock lock(&mutex_, true, 5000);
	if (!lock.IsLocked()) {
		NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		return -1;
	}
	try {
		return static_cast<int>(cpu.getAvrage(mseconds / (100)));
	} catch (PDHCollectors::PDHException &e) {
		NSC_LOG_ERROR(_T("Failed to get CPU value: ") + e.getError());
		return -1;
	} catch (...) {
		NSC_LOG_ERROR(_T("Failed to get CPU value"));
		return -1;
	}
}
/**
* Get uptime from counter
* @bug Do we need to collect this all the time ? (perhaps we can collect this in real time ?)
* @return uptime for the system
* @bug Are we overflow protected here ? (seem to recall some issues with overflow before ?)
*/
long long PDHCollector::getUptime() {
	ReadLock lock(&mutex_, true, 5000);
	if (!lock.IsLocked()) {
		NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		return -1;
	}
	try {
		return upTime.getValue();
	} catch (PDHCollectors::PDHException &e) {
		NSC_LOG_ERROR(_T("Failed to get UPTIME value: ") + e.getError());
		return -1;
	} catch (...) {
		NSC_LOG_ERROR(_T("Failed to get UPTIME value"));
		return -1;
	}
}
/**
* Memory commit limit (your guess is as good as mine to what this is :)
* @return Some form of memory check
*/
unsigned long long PDHCollector::getMemCommitLimit() {
	ReadLock lock(&mutex_, true, 5000);
	if (!lock.IsLocked()) {
		NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		return -1;
	}
	try {
		return memCmtLim.getValue();
	} catch (PDHCollectors::PDHException &e) {
		NSC_LOG_ERROR(_T("Failed to get MEM_CMT_LIMIT value: ") + e.getError());
		return -1;
	} catch (...) {
		NSC_LOG_ERROR(_T("Failed to get MEM_CMT_LIMIT value"));
		return -1;
	}
}
/**
*
* Memory committed bytes (your guess is as good as mine to what this is :)
* @return Some form of memory check
*/
unsigned long long PDHCollector::getMemCommit() {
	ReadLock lock(&mutex_, true, 5000);
	if (!lock.IsLocked()) {
		NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		return -1;
	}
	try {
		return memCmt.getValue();
	} catch (PDHCollectors::PDHException &e) {
		NSC_LOG_ERROR(_T("Failed to get MEM_CMT value: ") + e.getError());
		return -1;
	} catch (...) {
		NSC_LOG_ERROR(_T("Failed to get MEM_CMT value"));
		return -1;
	}
}
