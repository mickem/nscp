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
// Author: Michael Medin (mickem@medin.nu)
//
// This software is provided "AS IS", without a warranty of any kind.
// You are free to use/modify this code but leave this header intact.
//
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PDHCollector.h"


PDHCollector::PDHCollector() : hStopEvent_(NULL) {
	checkIntervall_ = NSCModuleHelper::getSettingsInt(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CHECK_RESOLUTION, C_SYSTEM_CHECK_RESOLUTION_DEFAULT);
	std::string s = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CPU_BUFFER_TIME, C_SYSTEM_CPU_BUFFER_TIME_DEFAULT);
	unsigned int i = strEx::stoui_as_time(s, checkIntervall_*100);
	cpu.resize(i/(checkIntervall_*100));
}

PDHCollector::~PDHCollector() 
{
	if (hStopEvent_)
		CloseHandle(hStopEvent_);
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
	PDH::PDHQuery pdh;
	pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_MEM_PAGE_LIMIT, C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT), &memCmtLim);
	pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_MEM_PAGE, C_SYSTEM_MEM_PAGE_DEFAULT), &memCmt);
	pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_UPTIME, C_SYSTEM_UPTIME_DEFAULT), &upTime);
	pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CPU, C_SYSTEM_MEM_CPU_DEFAULT), &cpu);

	try {
		pdh.open();
	} catch (const PDH::PDHException &e) {
		NSC_LOG_ERROR_STD("Failed to open performance counters: " + e.str_);
		return 0;
	}

	hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent_) {
		NSC_LOG_ERROR_STD("Create StopEvent failed: " + strEx::itos(GetLastError()));
		return 0;
	}

	do {
		MutexLock mutex(mutexHandler);
		if (!mutex.hasMutex()) 
			NSC_LOG_ERROR("Failed to get Mutex!");
		else {
			try {
				pdh.collect();
			} catch (const PDH::PDHException &e) {
				NSC_LOG_ERROR_STD("Failed to query performance counters: " + e.str_);
			}
		} 
	}while (!(WaitForSingleObject(hStopEvent_, checkIntervall_*100) == WAIT_OBJECT_0));

	{
		MutexLock mutex(mutexHandler);
		if (!mutex.hasMutex()) {
			NSC_LOG_ERROR("Failed to get Mute when closing thread!");
		}

		if (!CloseHandle(hStopEvent_))
			NSC_LOG_ERROR_STD("Failed to close stopEvent handle: " + strEx::itos(GetLastError()));
		else
			hStopEvent_ = NULL;
		try {
			pdh.close();
		} catch (const PDH::PDHException &e) {
			NSC_LOG_ERROR_STD("Failed to close performance counters: " + e.str_);
		}
	}
	return 0;
}


/**
 * Request termination of the thread (waiting for thread termination is not handled)
 */
void PDHCollector::exitThread(void) {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mute when trying to close thread!");
		return;
	}
	if (hStopEvent_ == NULL)
		NSC_LOG_ERROR("Failed to get stop event!");
	else
		if (!SetEvent(hStopEvent_)) {
			NSC_LOG_ERROR_STD("SetStopEvent failed");
	}
}
/**
 * Get the average CPU usage for "time"
 * @param time Time to check 
 * @return average CPU usage
 */
int PDHCollector::getCPUAvrage(std::string time) {
	unsigned int mseconds = strEx::stoui_as_time(time, checkIntervall_*100);
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return -1;
	}
	return cpu.getAvrage(mseconds / (checkIntervall_*100));
}
/**
 * Get uptime from counter
 * @bug Do we need to collect this all the time ? (perhaps we can collect this in real time ?)
 * @return uptime for the system
 * @bug Are we overflow protected here ? (seem to recall some issues with overflow before ?)
 */
long long PDHCollector::getUptime() {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return -1;
	}
	return upTime.getValue();
}
/**
 * Memory commit limit (your guess is as good as mine to what this is :)
 * @return Some form of memory check
 */
long long PDHCollector::getMemCommitLimit() {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return -1;
	}
	return memCmtLim.getValue();
}
/**
 *
 * Memory committed bytes (your guess is as good as mine to what this is :)
 * @return Some form of memory check
 */
long long PDHCollector::getMemCommit() {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return -1;
	}
	return memCmt.getValue();
}
