#pragma once

#include "PDHCollectors.h"
#include <thread.h>
#include <Mutex.h>

/**
 * @ingroup NSClientCompat
 * PDH collector thread (gathers performance data and allows for clients to retrieve it)
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 */
class PDHCollector {
private:
	MutexHandler mutexHandler;
	HANDLE hStopEvent_;
	int checkIntervall_;

	PDHCollectors::StaticPDHCounterListener<__int64, PDH_FMT_LARGE> memCmtLim;
	PDHCollectors::StaticPDHCounterListener<__int64, PDH_FMT_LARGE> memCmt;
	PDHCollectors::StaticPDHCounterListener<__int64, PDH_FMT_LARGE> upTime;
	PDHCollectors::RoundINTPDHBufferListener<__int64, PDH_FMT_LARGE> cpu;

public:
	PDHCollector();
	virtual ~PDHCollector();
	DWORD threadProc(LPVOID lpParameter);
	void exitThread(void);

	// Retrieve values
	int getCPUAvrage(std::string time);
	long long getUptime();
	long long getMemCommitLimit();
	long long getMemCommit();


private:
//	bool isRunning(void);
//	void startRunning(void);
//	void stopRunning(void);

};


typedef Thread<PDHCollector> PDHCollectorThread;