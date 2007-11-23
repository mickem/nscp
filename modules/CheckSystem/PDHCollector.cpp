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
	checkIntervall_ = NSCModuleHelper::getSettingsInt(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CHECK_RESOLUTION, C_SYSTEM_CHECK_RESOLUTION_DEFAULT);
	std::wstring s = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CPU_BUFFER_TIME, C_SYSTEM_CPU_BUFFER_TIME_DEFAULT);
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
	hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent_) {
		NSC_LOG_ERROR_STD(_T("Create StopEvent failed: ") + error::lookup::last_error());
		return 0;
	}
	PDH::PDHQuery pdh;


	if (NSCModuleHelper::getSettingsInt(C_SYSTEM_SECTION_TITLE, C_SYSTEM_AUTODETECT_PDH, C_SYSTEM_AUTODETECT_PDH_DEFAULT) == 1) {

		SettingsT settings;
		std::wstring prefix;
		settings.setFile(NSCModuleHelper::getBasePath(),  _T("counters.defs"), true);
		std::wstring section = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_FORCE_LANGUAGE, C_SYSTEM_FORCE_LANGUAGE_DEFAULT);

		try {
			OSVERSIONINFO osVer = systemInfo::getOSVersion();
			if (!systemInfo::isNTBased(osVer)) {
				NSC_LOG_ERROR_STD(_T("Detected Windows 3.x or Windows 9x, PDH will be disabled."));
				NSC_LOG_ERROR_STD(_T("To manual set performance counters you need to first set ") C_SYSTEM_AUTODETECT_PDH _T("=0 in the config file, and then you also need to configure the various counter."));
				return 0;
			}

			LANGID langId = -1;
			if (systemInfo::isBelowNT4(osVer)) {
				NSC_DEBUG_MSG_STD(_T("Autodetected NT4, using NT4 PDH counters."));
				prefix = _T("NT4");
				langId = systemInfo::GetSystemDefaultLangID();
			} else if (systemInfo::isAboveW2K(osVer)) {
				NSC_DEBUG_MSG_STD(_T("Autodetected w2k or later, using w2k PDH counters."));
				prefix = _T("W2K");
				langId = systemInfo::GetSystemDefaultUILanguage();
			} else {
				NSC_LOG_ERROR_STD(_T("Unknown OS detected, PDH will be disabled."));
				NSC_LOG_ERROR_STD(_T("To manual set performance counters you need to first set ") C_SYSTEM_AUTODETECT_PDH _T("=0 in the config file, and then you also need to configure the various counter."));
				return 0;
			}

			if (!section.empty()) {
				NSC_DEBUG_MSG_STD(_T("Overriding language with: ") + section);
			} else {
				section = _T("0000") + strEx::ihextos(langId);
				section = _T("0x") + section.substr(section.length()-4);
			}
			if (settings.getString(section, _T("Description"), _T("_NOT_FOUND")) == _T("_NOT_FOUND")) {
				NSC_LOG_ERROR_STD(_T("Detected language: ") + section + _T(" but it could not be found in: counters.defs"));
				NSC_LOG_ERROR_STD(_T("You need to manually configure performance counters!"));
				return 0;
			}
			NSC_DEBUG_MSG_STD(_T("Detected language: ") + settings.getString(section, _T("Description"), _T("Not found")) + _T(" (") + section + _T(")"));
		} catch (const systemInfo::SystemInfoException &e) {
			NSC_LOG_ERROR_STD(_T("To manual set performance counters you need to first set ") C_SYSTEM_AUTODETECT_PDH _T("=0 in the config file, and then you also need to configure the various counter."));
			return -1;
		}
		pdh.addCounter(settings.getString(section, prefix + _T("_") + C_SYSTEM_MEM_PAGE_LIMIT, C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT), &memCmtLim);
		pdh.addCounter(settings.getString(section, prefix + _T("_") + C_SYSTEM_MEM_PAGE, C_SYSTEM_MEM_PAGE_DEFAULT), &memCmt);
		pdh.addCounter(settings.getString(section, prefix + _T("_") + C_SYSTEM_UPTIME, C_SYSTEM_UPTIME_DEFAULT), &upTime);
		pdh.addCounter(settings.getString(section, prefix + _T("_") + C_SYSTEM_CPU, C_SYSTEM_MEM_CPU_DEFAULT), &cpu);
		try {
			pdh.open();
		} catch (const PDH::PDHException &e) {
			NSC_LOG_ERROR_STD(_T("Failed to open performance counters: ") + e.getError());
			NSC_LOG_ERROR_STD(_T("Trying to use default (English) counters"));
			pdh.removeAllCounters();
			pdh.addCounter(C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT, &memCmtLim);
			pdh.addCounter(C_SYSTEM_MEM_PAGE_DEFAULT, &memCmt);
			pdh.addCounter(C_SYSTEM_UPTIME_DEFAULT, &upTime);
			pdh.addCounter(C_SYSTEM_MEM_CPU_DEFAULT, &cpu);
			try {
				pdh.open();
			} catch (const PDH::PDHException &e) {
				NSC_LOG_ERROR_STD(_T("Failed to open default (English) performance counters: ") + e.getError());
				NSC_LOG_ERROR_STD(_T("We will now terminate the collection thread!"));
				return 0;
			}
		}
	} else {
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_MEM_PAGE_LIMIT, C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT), &memCmtLim);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_MEM_PAGE, C_SYSTEM_MEM_PAGE_DEFAULT), &memCmt);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_UPTIME, C_SYSTEM_UPTIME_DEFAULT), &upTime);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CPU, C_SYSTEM_MEM_CPU_DEFAULT), &cpu);

		try {
			pdh.open();
		} catch (const PDH::PDHException &e) {
			NSC_LOG_ERROR_STD(_T("Failed to open performance counters: ") + e.getError());
			return 0;
		}
	}

	DWORD waitStatus = 0;
	bool first = true;
	do {
		MutexLock mutex(mutexHandler);
		if (!mutex.hasMutex()) 
			NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		else {
			try {
				pdh.gatherData();
			} catch (const PDH::PDHException &e) {
				if (first) {	// If this is the first run an error will be thrown since the data is not yet avalible
								// This is "ok" but perhaps another solution would be better, but this works :)
					first = false;
				} else {
					NSC_LOG_ERROR_STD(_T("Failed to query performance counters: ") + e.getError());
				}
			}
		} 
	}while (((waitStatus = WaitForSingleObject(hStopEvent_, checkIntervall_*100)) == WAIT_TIMEOUT));
	if (waitStatus != WAIT_OBJECT_0) {
		NSC_LOG_ERROR(_T("Something odd happened, terminating PDH collection thread!"));
	}

	{
		MutexLock mutex(mutexHandler);
		if (!mutex.hasMutex()) {
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


/**
* Request termination of the thread (waiting for thread termination is not handled)
*/
void PDHCollector::exitThread(void) {
	if (hStopEvent_ == NULL)
		NSC_LOG_ERROR(_T("Stop event is not created!"));
	else
		if (!SetEvent(hStopEvent_)) {
			NSC_LOG_ERROR_STD(_T("SetStopEvent failed"));
		}
}
/**
* Get the average CPU usage for "time"
* @param time Time to check 
* @return average CPU usage
*/
int PDHCollector::getCPUAvrage(std::wstring time) {
	unsigned int mseconds = strEx::stoui_as_time(time, checkIntervall_*100);
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		return -1;
	}
	return static_cast<int>(cpu.getAvrage(mseconds / (checkIntervall_*100)));
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
		NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		return -1;
	}
	return upTime.getValue();
}
/**
* Memory commit limit (your guess is as good as mine to what this is :)
* @return Some form of memory check
*/
unsigned long long PDHCollector::getMemCommitLimit() {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		return -1;
	}
	return memCmtLim.getValue();
}
/**
*
* Memory committed bytes (your guess is as good as mine to what this is :)
* @return Some form of memory check
*/
unsigned long long PDHCollector::getMemCommit() {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		return -1;
	}
	return memCmt.getValue();
}
