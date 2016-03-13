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
#include "TaskSched.h"

#include <atlbase.h>

#include <error_com.hpp>
#include <error.hpp>

#include <map>
#include <comdef.h>

#include <error_com.hpp>
#include <objidl.h>
#include <map>
#include <Mstask.h>
#include <taskschd.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")

#define TASKS_TO_RETRIEVE 5

void find_old(tasksched_filter::filter &filter) {
	CComPtr<ITaskScheduler> taskSched;
	HRESULT hr = CoCreateInstance(CLSID_CTaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskScheduler, reinterpret_cast<void**>(&taskSched));
	if (FAILED(hr)) {
		throw nscp_exception("CoCreateInstance for CLSID_CTaskScheduler failed: " + error::com::get(hr));
	}

	CComPtr<IEnumWorkItems> taskSchedEnum;
	hr = taskSched->Enum(&taskSchedEnum);
	if (FAILED(hr)) {
		throw nscp_exception("Failed to enum work items: " + error::com::get(hr));
	}

	LPWSTR *lpwszNames;
	DWORD dwFetchedTasks = 0;
	while (SUCCEEDED(taskSchedEnum->Next(TASKS_TO_RETRIEVE, &lpwszNames, &dwFetchedTasks)) && (dwFetchedTasks != 0)) {
		while (dwFetchedTasks) {
			CComPtr<ITask> task;
			std::string title = utf8::cvt<std::string>(lpwszNames[--dwFetchedTasks]);
			taskSched->Activate(lpwszNames[dwFetchedTasks], IID_ITask, reinterpret_cast<IUnknown**>(&task));
			CoTaskMemFree(lpwszNames[dwFetchedTasks]);
			boost::shared_ptr<tasksched_filter::filter_obj> record(new tasksched_filter::old_filter_obj((ITask*)task, title));
			modern_filter::match_result ret = filter.match(record);
			if (ret.is_done) {
				break;
			}
		}
		CoTaskMemFree(lpwszNames);
	}
}

void do_get(CComPtr<ITaskService> taskSched, tasksched_filter::filter &filter, std::string folder, bool recursive);

void TaskSched::findAll(tasksched_filter::filter &filter, std::string computer, std::string user, std::string domain, std::string password, std::string folder, bool recursive) {
	CComPtr<ITaskService> taskSched;
	HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(&taskSched));
	if (FAILED(hr)) {
		NSC_DEBUG_MSG("Failed to create mordern finder using old method: " + error::com::get(hr));
		return find_old(filter);
	}
	taskSched->Connect(_variant_t(utf8::cvt<std::wstring>(computer).c_str()), _variant_t(utf8::cvt<std::wstring>(user).c_str()),
		_variant_t(utf8::cvt<std::wstring>(domain).c_str()), _variant_t(utf8::cvt<std::wstring>(password).c_str()));
	do_get(taskSched, filter, folder, recursive);
}

void do_get(CComPtr<ITaskService> taskSched, tasksched_filter::filter &filter, std::string folder, bool recursive) {
	CComPtr<ITaskFolder> pRootFolder;
	HRESULT hr = taskSched->GetFolder(_bstr_t(utf8::cvt<std::wstring>(folder).c_str()), &pRootFolder);
	if (FAILED(hr)) {
		throw nscp_exception("Failed to get folder " + folder + ": " + error::com::get(hr));
	}

	if (recursive) {
		CComPtr<ITaskFolderCollection> folders;
		if (FAILED(pRootFolder->GetFolders(0, &folders)))
			throw nscp_exception("Failed to get folder " + folder + ": " + error::com::get(hr));
		LONG count = 0;
		if (FAILED(folders->get_Count(&count)))
			throw nscp_exception("Failed to get folder " + folder + ": " + error::com::get(hr));
		std::vector<std::string> sub_folders;
		for (LONG i = 0; i < count; ++i) {
			CComPtr<ITaskFolder> inst;
			if (FAILED(folders->get_Item(_variant_t(i + 1), &inst)))
				throw nscp_exception("Failed to get folder " + folder + ": " + error::com::get(hr));
			BSTR str;
			if (FAILED(inst->get_Path(&str)))
				throw nscp_exception("Failed to get folder " + folder + ": " + error::com::get(hr));
			_bstr_t sstr(str, FALSE);
			sub_folders.push_back(utf8::cvt<std::string>(std::wstring(sstr)));
		}
	}

	CComPtr<IRegisteredTaskCollection> pTaskCollection;
	hr = pRootFolder->GetTasks(NULL, &pTaskCollection);
	if (FAILED(hr)) {
		throw nscp_exception("Failed to enum work items failed: " + error::com::get(hr));
	}

	LONG numTasks = 0;
	hr = pTaskCollection->get_Count(&numTasks);
	if (FAILED(hr)) {
		throw nscp_exception("Failed to get count: " + error::com::get(hr));
	}

	if (numTasks == 0) {
		return;
	}

	TASK_STATE taskState;

	for (LONG i = 0; i < numTasks; i++) {
		CComPtr<IRegisteredTask> pRegisteredTask = NULL;
		hr = pTaskCollection->get_Item(_variant_t(i + 1), &pRegisteredTask);
		if (SUCCEEDED(hr)) {
			boost::shared_ptr<tasksched_filter::filter_obj> record(new tasksched_filter::new_filter_obj((IRegisteredTask*)pRegisteredTask, folder));
			modern_filter::match_result ret = filter.match(record);
			if (ret.is_done) {
				break;
			}
		}
	}
}