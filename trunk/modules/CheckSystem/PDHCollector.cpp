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
#include <Settings.h>
#include <sysinfo.h>


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
	hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent_) {
		NSC_LOG_ERROR_STD("Create StopEvent failed: " + strEx::itos(GetLastError()));
		return 0;
	}
	PDH::PDHQuery pdh;


	if (NSCModuleHelper::getSettingsInt(C_SYSTEM_SECTION_TITLE, C_SYSTEM_AUTODETECT_PDH, C_SYSTEM_AUTODETECT_PDH_DEFAULT) == 1) {

		SettingsT settings;
		std::string prefix;
		std::string section;
		settings.setFile(NSCModuleHelper::getBasePath() + "\\counters.defs", true);

		NSC_LOG_ERROR_STD("Getting counter info...");

		try {
			OSVERSIONINFO osVer = systemInfo::getOSVersion();
			if (!systemInfo::isNTBased(osVer)) {
				NSC_LOG_ERROR_STD("Detected Windows 3.x or Windows 9x, PDH will be disabled.");
				NSC_LOG_ERROR_STD("To manual set performance counters you need to first set " C_SYSTEM_AUTODETECT_PDH "=0 in the config file, and then you also need to configure the various counter.");
				return 0;
			}

			LANGID langId = -1;
			if (systemInfo::isBelowNT4(osVer)) {
				NSC_DEBUG_MSG_STD("Autodetected NT4, using NT4 PDH counters.");
				prefix = "NT4";
				langId = systemInfo::GetSystemDefaultLangID();
			} else if (systemInfo::isAboveW2K(osVer)) {
				NSC_DEBUG_MSG_STD("Autodetected w2k or later, using w2k PDH counters.");
				prefix = "W2K";
				langId = systemInfo::GetSystemDefaultUILanguage();
			} else {
				NSC_LOG_ERROR_STD("Unknown OS detected, PDH will be disabled.");
				NSC_LOG_ERROR_STD("To manual set performance counters you need to first set " C_SYSTEM_AUTODETECT_PDH "=0 in the config file, and then you also need to configure the various counter.");
				return 0;
			}

			section = "0000" + strEx::ihextos(langId);
			section = "0x" + section.substr(section.length()-4);
			if (settings.getString(section, "Description", "_NOT_FOUND") == "_NOT_FOUND") {
				NSC_LOG_ERROR_STD("Detected language: " + section + " but it could not be found in: counters.defs");
				NSC_LOG_ERROR_STD("You need to manually configure performance counters!");
				return 0;
			}
			NSC_DEBUG_MSG_STD("Detected language: " + settings.getString(section, "Description", "Not found") + " (" + section + ")");
		} catch (systemInfo::SystemInfoException e) {
			NSC_LOG_ERROR_STD("System detection failed, PDH will be disabled: " + e.error_);
			NSC_LOG_ERROR_STD("To manual set performance counters you need to first set " C_SYSTEM_AUTODETECT_PDH "=0 in the config file, and then you also need to configure the various counter.");
			return -1;
		}

		pdh.addCounter(settings.getString(section, prefix + "_" + C_SYSTEM_MEM_PAGE_LIMIT, C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT), &memCmtLim);
		pdh.addCounter(settings.getString(section, prefix + "_" + C_SYSTEM_MEM_PAGE, C_SYSTEM_MEM_PAGE_DEFAULT), &memCmt);
		pdh.addCounter(settings.getString(section, prefix + "_" + C_SYSTEM_UPTIME, C_SYSTEM_UPTIME_DEFAULT), &upTime);
		pdh.addCounter(settings.getString(section, prefix + "_" + C_SYSTEM_CPU, C_SYSTEM_MEM_CPU_DEFAULT), &cpu);
	} else {
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_MEM_PAGE_LIMIT, C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT), &memCmtLim);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_MEM_PAGE, C_SYSTEM_MEM_PAGE_DEFAULT), &memCmt);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_UPTIME, C_SYSTEM_UPTIME_DEFAULT), &upTime);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CPU, C_SYSTEM_MEM_CPU_DEFAULT), &cpu);
	}

	NSC_LOG_ERROR_STD("Attempting to open counter...");
	try {
		pdh.open();
		NSC_LOG_ERROR_STD("Counters opend...");
	} catch (const PDH::PDHException &e) {
		NSC_LOG_ERROR_STD("Failed to open performance counters: " + e.getError());
		return 0;
	}


	DWORD waitStatus = 0;
	bool first = true;
	do {
		MutexLock mutex(mutexHandler);
		if (!mutex.hasMutex()) 
			NSC_LOG_ERROR("Failed to get Mutex!");
		else {
			try {
				pdh.gatherData();
			} catch (const PDH::PDHException &e) {
				if (first) {	// If this is the first run an error will be thrown since the data is not yet avalible
								// This is "ok" but perhaps another solution would be better, but this works :)
					first = false;
				} else {
					NSC_LOG_ERROR_STD("Failed to query performance counters: " + e.getError());
				}
			}
		} 
	}while (((waitStatus = WaitForSingleObject(hStopEvent_, checkIntervall_*100)) == WAIT_TIMEOUT));
	if (waitStatus != WAIT_OBJECT_0) {
		NSC_LOG_ERROR("Something odd happened, terminating PDH collection thread!");
	}

	{
		MutexLock mutex(mutexHandler);
		if (!mutex.hasMutex()) {
			NSC_LOG_ERROR("Failed to get Mute when closing thread!");
		}

		if (!CloseHandle(hStopEvent_)) {
			NSC_LOG_ERROR_STD("Failed to close stopEvent handle: " + strEx::itos(GetLastError()));
		} else
			hStopEvent_ = NULL;
		try {
			pdh.close();
		} catch (const PDH::PDHException &e) {
			NSC_LOG_ERROR_STD("Failed to close performance counters: " + e.getError());
		}
	}
	return 0;
}


/**
* Request termination of the thread (waiting for thread termination is not handled)
*/
void PDHCollector::exitThread(void) {
	if (hStopEvent_ == NULL)
		NSC_LOG_ERROR("Stop event is not created!");
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
		NSC_LOG_ERROR("Failed to get Mutex!");
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
unsigned long long PDHCollector::getMemCommit() {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return -1;
	}
	return memCmt.getValue();
}
