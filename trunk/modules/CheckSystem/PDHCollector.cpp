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

bool PDHCollector::loadCounter(PDH::PDHQuery &pdh) {
	if (NSCModuleHelper::getSettingsInt(C_SYSTEM_SECTION_TITLE, C_SYSTEM_AUTODETECT_PDH, C_SYSTEM_AUTODETECT_PDH_DEFAULT) != 1) {
		NSC_DEBUG_MSG_STD(_T("Autodetect disabled from nsc.ini via: ") + C_SYSTEM_AUTODETECT_PDH);
		return false;
	}
	std::wstring prefix;
	std::wstring section = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_FORCE_LANGUAGE, C_SYSTEM_FORCE_LANGUAGE_DEFAULT);
	int noIndex = NSCModuleHelper::getSettingsInt(C_SYSTEM_SECTION_TITLE, C_SYSTEM_NO_INDEX, C_SYSTEM_NO_INDEX_DEFAULT);
	bool bUseIndex = false;

	// Investigate enviornment and find out what to use
	try {
		OSVERSIONINFO osVer = systemInfo::getOSVersion();
		if (!systemInfo::isNTBased(osVer)) {
			NSC_LOG_ERROR_STD(_T("Detected Windows 3.x or Windows 9x, PDH will be disabled."));
			NSC_LOG_ERROR_STD(_T("To manual set performance counters you need to first set ") C_SYSTEM_AUTODETECT_PDH _T("=0 in the config file, and then you also need to configure the various counter."));
			return false;
		}

		LANGID langId = -1;
		if (systemInfo::isBelowNT4(osVer)) {
			NSC_DEBUG_MSG_STD(_T("Autodetected NT4, using NT4 PDH counters."));
			prefix = _T("NT4");
			bUseIndex = false;
			langId = systemInfo::GetSystemDefaultLangID();
		} else if (systemInfo::isAboveW2K(osVer)) {
			NSC_DEBUG_MSG_STD(_T("Autodetected w2k or later, using w2k PDH counters."));
			bUseIndex = true;
			prefix = _T("W2K");
			langId = systemInfo::GetSystemDefaultUILanguage();
		} else {
			NSC_LOG_ERROR_STD(_T("Unknown OS detected, PDH will be disabled."));
			NSC_LOG_ERROR_STD(_T("To manual set performance counters you need to first set ") C_SYSTEM_AUTODETECT_PDH _T("=0 in the config file, and then you also need to configure the various counter."));
			return false;
		}

		if (!section.empty()) {
			NSC_DEBUG_MSG_STD(_T("Overriding language with: ") + section);
		} else {
			section = _T("0000") + strEx::ihextos(langId);
			section = _T("0x") + section.substr(section.length()-4);
		}
		if (bUseIndex&&noIndex==1) {
			NSC_DEBUG_MSG_STD(_T("We wanted to use index but were forced not to use them due to: ") + C_SYSTEM_NO_INDEX);
			bUseIndex = false;
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
		std::wstring proc;
		std::wstring uptime;
		std::wstring memCl;
		std::wstring memCb;
		if (bUseIndex) {
			NSC_DEBUG_MSG_STD(_T("Using index to retrive counternames"));
			proc = _T("\\") + pdh.lookupIndex(238) + _T("(_total)\\") + pdh.lookupIndex(6);
			uptime = _T("\\") + pdh.lookupIndex(2) + _T("\\") + pdh.lookupIndex(674);
			memCl = _T("\\") + pdh.lookupIndex(4) + _T("\\") + pdh.lookupIndex(30);
			memCb = _T("\\") + pdh.lookupIndex(4) + _T("\\") + pdh.lookupIndex(26);
		} else {
			SettingsT settings;
			settings.setFile(NSCModuleHelper::getBasePath(),  _T("counters.defs"), true);
			NSC_DEBUG_MSG_STD(_T("Detected language: ") + settings.getString(section, _T("Description"), _T("Not found")) + _T(" (") + section + _T(")"));
			if (settings.getString(section, _T("Description"), _T("_NOT_FOUND")) == _T("_NOT_FOUND")) {
				NSC_LOG_ERROR_STD(_T("Detected language: ") + section + _T(" but it could not be found in: counters.defs"));
				NSC_LOG_ERROR_STD(_T("You need to manually configure performance counters!"));
				return false;
			}
			NSC_DEBUG_MSG_STD(_T("Attempting to get localized PDH values from the .defs file"));
			proc = settings.getString(section, prefix + _T("_") + C_SYSTEM_CPU, C_SYSTEM_MEM_CPU_DEFAULT);
			uptime = settings.getString(section, prefix + _T("_") + C_SYSTEM_UPTIME, C_SYSTEM_UPTIME_DEFAULT);
			memCl = settings.getString(section, prefix + _T("_") + C_SYSTEM_MEM_PAGE_LIMIT, C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT);
			memCb = settings.getString(section, prefix + _T("_") + C_SYSTEM_MEM_PAGE, C_SYSTEM_MEM_PAGE_DEFAULT);
		}
		NSC_DEBUG_MSG_STD(_T("Found countername: CPU:    ") + proc);
		NSC_DEBUG_MSG_STD(_T("Found countername: UPTIME: ") + uptime);
		NSC_DEBUG_MSG_STD(_T("Found countername: MCL:    ") + memCl);
		NSC_DEBUG_MSG_STD(_T("Found countername: MCB:    ") + memCb);
		pdh.addCounter(proc, &cpu);
		pdh.addCounter(uptime, &upTime);
		pdh.addCounter(memCl, &memCmtLim);
		pdh.addCounter(memCb, &memCmt);
		pdh.open();
	} catch (const PDH::PDHException &e) {
		NSC_LOG_ERROR_STD(_T("Failed to open performance counters: ") + e.getError());
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
	hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent_) {
		NSC_LOG_ERROR_STD(_T("Create StopEvent failed: ") + error::lookup::last_error());
		return 0;
	}
	PDH::PDHQuery pdh;
	bool bInit = true;

	if (!loadCounter(pdh)) {
		pdh.removeAllCounters();
		NSC_DEBUG_MSG_STD(_T("We aparently failed to load counters trying to use default (English) counters or those configured in nsc.ini"));
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),SORT_DEFAULT));
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_MEM_PAGE_LIMIT, C_SYSTEM_MEM_PAGE_LIMIT_DEFAULT), &memCmtLim);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_MEM_PAGE, C_SYSTEM_MEM_PAGE_DEFAULT), &memCmt);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_UPTIME, C_SYSTEM_UPTIME_DEFAULT), &upTime);
		pdh.addCounter(NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CPU, C_SYSTEM_MEM_CPU_DEFAULT), &cpu);
		try {
			pdh.open();
		} catch (const PDH::PDHException &e) {
			NSC_LOG_ERROR_STD(_T("Failed to open performance counters: ") + e.getError());
			bInit = false;
		}
	}

	DWORD waitStatus = 0;
	if (bInit) {
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
		} while (((waitStatus = WaitForSingleObject(hStopEvent_, checkIntervall_*100)) == WAIT_TIMEOUT));
	} else {
		NSC_LOG_ERROR_STD(_T("No performance counters were found we will not wait for the end instead..."));
		waitStatus = WaitForSingleObject(hStopEvent_, INFINITE);
	}
	if (waitStatus != WAIT_OBJECT_0) {
		NSC_LOG_ERROR(_T("Something odd happened when terminating PDH collection thread!"));
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
