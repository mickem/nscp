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


PDHCollector::PDHCollector() : cpu(BACK_INTERVAL*60/CHECK_INTERVAL), running_(true) {
}
PDHCollector::~PDHCollector() 
{
}

/**
 * Check running status (mutex locked)
 * @return current status of the running flag (or false if we could  not get the mutex, though this is most likely a critical state)
 */
bool PDHCollector::isRunning(void) {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return false;
	}
	return running_;
}
/**
 * set running status (to stopped) 
 */
void PDHCollector::stopRunning(void) {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return;
	}
	running_ = false;
}
/**
 *set running status (to started) 
 */
void PDHCollector::startRunning(void) {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return;
	}
	running_ = true;
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
 * @bug As this is now we have to wait for CHECK_INTERVAL seconds before the service terminates (should probably be changed into some form of event)
 * @bug If we have "custom named" counters ?
 * @bug This whole concept needs work I think.
 *
 */
DWORD PDHCollector::threadProc(LPVOID lpParameter) {
	PDH::PDHQuery pdh;
	pdh.addCounter("\\\\.\\Memory\\Commit Limit", &memCmtLim);
	pdh.addCounter("\\\\.\\Memory\\Committed Bytes", &memCmt);
	pdh.addCounter("\\\\.\\System\\System Up Time", &upTime);
	pdh.addCounter("\\\\.\\Processor(_total)\\% Processor Time", &cpu);

	try {
		pdh.open();
	} catch (const PDH::PDHException &e) {
		NSC_LOG_ERROR_STD("Failed to open performance counters: " + e.str_);
		return 0;
	}

	startRunning();
	while(isRunning()) {
		{
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
		}
		Sleep(CHECK_INTERVAL*1000);
	}

	try {
		pdh.close();
	} catch (const PDH::PDHException &e) {
		NSC_LOG_ERROR_STD("Failed to close performance counters: " + e.str_);
	}
	NSC_DEBUG_MSG("PDHCollector - shutdown!");
	return 0;
}


/**
 * Request termination of the thread (waiting for thread termination is not handled)
 */
void PDHCollector::exitThread(void) {
	NSC_DEBUG_MSG("PDHCollector - Requesting shutdown!");
	stopRunning();
}
/**
 * Get the average CPU usage for "time"
 * @param backItems 
 * @return average CPU usage
 */
int PDHCollector::getCPUAvrage(unsigned int backItems) {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return -1;
	}
	return cpu.getAvrage(backItems);
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
