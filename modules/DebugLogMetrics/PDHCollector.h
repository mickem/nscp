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

#include <pdh/collectors.hpp>
#include <pdh/query.hpp>
#include <thread.h>
#include <MutexRW.h>
#include <file_logger.hpp>
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

	struct pdh_value {
		pdh_value(int index_) : index(index_) {}
		std::wstring key;
		int index;
		PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> value;
	};
	typedef std::list<pdh_value*> pdh_list;
	pdh_list list_;

	std::wstring process_name_;
	MutexRW mutex_;
	HANDLE hStopEvent_;
	HANDLE hStoreEvent_;
	simple_file::file_appender file_;
	std::wstring separator_;
	std::wstring event_;
/*
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> handle_count_;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> thread_count_;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> pid_;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> virt_mem_;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> top_virt_mem_;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> top_active_page_;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> active_page_;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> top_swap_mem_;
	PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large, PDHCollectors::PDHCounterNormalMutex> swap_mem_;
*/
public:
	PDHCollector();
	virtual ~PDHCollector();
	DWORD threadProc(LPVOID lpParameter);
	void exitThread(void);

	// Retrieve values
	bool loadCounter(PDH::PDHQuery &pdh);
	void store(std::wstring event);


private:
//	bool isRunning(void);
//	void startRunning(void);
//	void stopRunning(void);

};


typedef Thread<PDHCollector> PDHCollectorThread;