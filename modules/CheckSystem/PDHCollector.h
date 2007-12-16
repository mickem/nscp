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

	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large> memCmtLim;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large> memCmt;
	PDHCollectors::StaticPDHCounterListener<__int64, PDHCollectors::format_large> upTime;
	PDHCollectors::RoundINTPDHBufferListener<__int64, PDHCollectors::format_large> cpu;

public:
	PDHCollector();
	virtual ~PDHCollector();
	DWORD threadProc(LPVOID lpParameter);
	void exitThread(void);

	// Retrieve values
	int getCPUAvrage(std::wstring time);
	long long getUptime();
	unsigned long long getMemCommitLimit();
	unsigned long long getMemCommit();
	bool loadCounter(PDH::PDHQuery &pdh);


private:
//	bool isRunning(void);
//	void startRunning(void);
//	void stopRunning(void);

};


typedef Thread<PDHCollector> PDHCollectorThread;