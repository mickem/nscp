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
#include ".\TaskSched.h"

#include <objidl.h>
#include <map>

std::wstring TaskSched::sanitize_string(LPTSTR in) {
	TCHAR *p = in;
	while (*p) {
		if (p[0] < ' ' || p[0] > '}')
			p[0] = '.';
		p++;
	} 
	return in;
}
#define TASKS_TO_RETRIEVE 5

#define FETCH_TASK_SIMPLE_STR(variable, func) \
	if (key.variable) { \
		LPWSTR lpwszStr; \
		HRESULT hr = task->func(&lpwszStr); \
		if (FAILED(hr)) { \
			res.variable = _T("ERROR: ") + error::format::from_system(hr); \
		} else { \
			res.variable = lpwszStr; \
			CoTaskMemFree(lpwszStr); \
		} \
	} 
#define FETCH_TASK_SIMPLE_DWORD(variable, func, errVal) \
	if (key.variable) { \
	if (FAILED(task->func(&res.variable))) \
	res.variable = errVal; \
	} 


unsigned long long systemtime_to_ullFiletime(SYSTEMTIME time) {
	FILETIME FileTime;
	SystemTimeToFileTime(&time, &FileTime);
	return ((FileTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)FileTime.dwLowDateTime);
}


#define FETCH_TASK_SIMPLE_TIME(variable, func) \
	if (key.variable) { \
	SYSTEMTIME st; \
	HRESULT hr = task->func(&st); \
	if (hr == SCHED_S_TASK_HAS_NOT_RUN) \
		res.variable.never_ = true; \
	else if (FAILED(hr)) \
		throw Exception(_T("Failed to get value for: ") _T(# variable), hr); \
	else if (SUCCEEDED(hr)) {\
		res.variable = systemtime_to_ullFiletime(st); \
}}

TaskSched::result_type TaskSched::findAll(result::fetch_key key)
{
	result_type ret;

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
	FILETIME now_;
	GetSystemTimeAsFileTime(&now_);
	unsigned long long now = ((now_.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now_.dwLowDateTime);

	LPWSTR *lpwszNames;
	DWORD dwFetchedTasks = 0;
	while (SUCCEEDED(taskSchedEnum->Next(TASKS_TO_RETRIEVE, &lpwszNames, &dwFetchedTasks)) && (dwFetchedTasks != 0)) {
		while (dwFetchedTasks) {
			CComPtr<ITask> task;
			result res;
			res.title = lpwszNames[--dwFetchedTasks];
			taskSched->Activate(lpwszNames[dwFetchedTasks], IID_ITask, reinterpret_cast<IUnknown**>(&task));
			CoTaskMemFree(lpwszNames[dwFetchedTasks]);
			res.ullNow = now;

			FETCH_TASK_SIMPLE_STR(accountName, GetAccountInformation);
			FETCH_TASK_SIMPLE_STR(applicationName, GetApplicationName);
			FETCH_TASK_SIMPLE_STR(comment, GetComment);
			FETCH_TASK_SIMPLE_STR(creator, GetCreator);
			//FETCH_TASK_SIMPLE_DWORD(errorRetryCount, GetErrorRetryCount, 0);
			//FETCH_TASK_SIMPLE_DWORD(errorRetryInterval, GetErrorRetryInterval, 0);
			FETCH_TASK_SIMPLE_DWORD(exitCode, GetExitCode, 0);
			FETCH_TASK_SIMPLE_DWORD(flags, GetFlags, 0);
			//FETCH_TASK_SIMPLE_DWORD(flags, GetIdleWait, 0);
			FETCH_TASK_SIMPLE_DWORD(flags, GetMaxRunTime, 0);
			FETCH_TASK_SIMPLE_TIME(mostRecentRunTime,GetMostRecentRunTime);
			FETCH_TASK_SIMPLE_TIME(nextRunTime,GetNextRunTime);

			FETCH_TASK_SIMPLE_STR(parameters, GetParameters);
			FETCH_TASK_SIMPLE_DWORD(priority, GetPriority, 0);
			FETCH_TASK_SIMPLE_DWORD(status, GetStatus, 0);
			// Trigger
			FETCH_TASK_SIMPLE_STR(workingDirectory, GetWorkingDirectory);
			//FETCH_TASK_SIMPLE_STR(data, GetWorkItemData);
			ret.push_back(res);
		}
		CoTaskMemFree(lpwszNames);
	}
	return ret;
}
