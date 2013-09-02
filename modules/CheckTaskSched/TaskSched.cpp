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
#include "StdAfx.h"
#include "TaskSched.h"

#include <error_com.hpp>
#include <objidl.h>
#include <map>

#define TASKS_TO_RETRIEVE 5

void TaskSched::findAll(tasksched_filter::filter &filter) {
	CComPtr<ITaskScheduler> taskSched;
	HRESULT hr = CoCreateInstance( CLSID_CTaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskScheduler, reinterpret_cast<void**>(&taskSched));
	if (FAILED(hr)) {
		throw Exception(_T("CoCreateInstance for CLSID_CTaskScheduler failed!"), hr);
	}

	CComPtr<IEnumWorkItems> taskSchedEnum;
	hr = taskSched->Enum(&taskSchedEnum);
	if (FAILED(hr)) {
		throw Exception(_T("Failed to enum work items failed!"), hr);
	}

	LPWSTR *lpwszNames;
	DWORD dwFetchedTasks = 0;
	while (SUCCEEDED(taskSchedEnum->Next(TASKS_TO_RETRIEVE, &lpwszNames, &dwFetchedTasks)) && (dwFetchedTasks != 0)) {
		while (dwFetchedTasks) {
			CComPtr<ITask> task;
			std::string title = utf8::cvt<std::string>(lpwszNames[--dwFetchedTasks]);
			taskSched->Activate(lpwszNames[dwFetchedTasks], IID_ITask, reinterpret_cast<IUnknown**>(&task));
			CoTaskMemFree(lpwszNames[dwFetchedTasks]);
			boost::shared_ptr<tasksched_filter::filter_obj> record(new tasksched_filter::filter_obj((ITask*)task, title));
			boost::tuple<bool,bool> ret = filter.match(record);
			if (ret.get<1>()) {
				break;
			}
		}
		CoTaskMemFree(lpwszNames);
	}
}
