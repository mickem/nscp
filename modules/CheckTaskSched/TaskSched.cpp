/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "TaskSched.h"

#include <atlbase.h>

#include <error/error_com.hpp>

#include <map>
#include <comdef.h>

#include <error/error_com.hpp>
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
		throw nsclient::nsclient_exception("CoCreateInstance for CLSID_CTaskScheduler failed: " + error::com::get(hr));
	}

	CComPtr<IEnumWorkItems> taskSchedEnum;
	hr = taskSched->Enum(&taskSchedEnum);
	if (FAILED(hr)) {
		throw nsclient::nsclient_exception("Failed to enum work items: " + error::com::get(hr));
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
		}
		CoTaskMemFree(lpwszNames);
	}
}

void do_get(CComPtr<ITaskService> taskSched, tasksched_filter::filter &filter, std::string folder, bool recursive, bool hidden);

void TaskSched::findAll(tasksched_filter::filter &filter, std::string computer, std::string user, std::string domain, std::string password, std::string folder, bool recursive, bool hidden, bool old) {
	if (old) {
		return find_old(filter);
	}
	CComPtr<ITaskService> taskSched;
	HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(&taskSched));
	if (FAILED(hr)) {
		NSC_DEBUG_MSG("Failed to create mordern finder using old method: " + error::com::get(hr));
		return find_old(filter);
	}
	_variant_t vComputer;
	if (!computer.empty())
		vComputer = utf8::cvt<std::wstring>(computer).c_str();
	_variant_t vUser;
	if (!user.empty())
		vUser = utf8::cvt<std::wstring>(user).c_str();
	_variant_t vDomain;
	if (!domain.empty())
		vDomain = utf8::cvt<std::wstring>(domain).c_str();
	_variant_t vPassword;
	if (password.empty())
		vPassword = utf8::cvt<std::wstring>(password).c_str();
	hr = taskSched->Connect(vComputer, vUser, vDomain, vPassword);
	
	if (FAILED(hr)) {
		NSC_DEBUG_MSG("Failed to connect to: computer: '" + computer + "', domain: '" + domain + "', user: '" + user + "', password: '" + std::string(password.size(), '*') + "': " + str::xtos(hr));
		throw nsclient::nsclient_exception("Failed to connect to task service on " + computer + ": " + error::com::get(hr));
	}
	do_get(taskSched, filter, folder, recursive, hidden);
}

void do_get(CComPtr<ITaskService> taskSched, tasksched_filter::filter &filter, std::string folder, bool recursive, bool hidden) {
	CComPtr<ITaskFolder> pRootFolder;
	HRESULT hr = taskSched->GetFolder(_bstr_t(utf8::cvt<std::wstring>(folder).c_str()), &pRootFolder);
	if (FAILED(hr)) {
		throw nsclient::nsclient_exception("Failed to get root folder " + folder + ": " + error::com::get(hr));
	}

	std::vector<std::string> sub_folders;
	if (recursive) {
		CComPtr<ITaskFolderCollection> folders;
		if (FAILED(pRootFolder->GetFolders(0, &folders)))
			throw nsclient::nsclient_exception("Failed to get folders below " + folder + ": " + error::com::get(hr));
		LONG count = 0;
		if (FAILED(folders->get_Count(&count)))
			throw nsclient::nsclient_exception("Failed to get count of folders below " + folder + ": " + error::com::get(hr));
		for (LONG i = 0; i < count; ++i) {
			CComPtr<ITaskFolder> inst;
			if (FAILED(folders->get_Item(_variant_t(i + 1), &inst)))
				throw nsclient::nsclient_exception("Failed to get folder item " + str::xtos(i) + ": " + error::com::get(hr));
			BSTR str;
			if (FAILED(inst->get_Path(&str)))
				throw nsclient::nsclient_exception("Failed to get path for " + str::xtos(i) + ": " + error::com::get(hr));
			_bstr_t sstr(str, FALSE);
			sub_folders.push_back(utf8::cvt<std::string>(std::wstring(sstr)));
		}
	}

	CComPtr<IRegisteredTaskCollection> pTaskCollection;
	hr = pRootFolder->GetTasks(hidden?TASK_ENUM_HIDDEN:NULL, &pTaskCollection);
	if (FAILED(hr)) {
		throw nsclient::nsclient_exception("Failed to enum work items failed: " + error::com::get(hr));
	}

	LONG numTasks = 0;
	hr = pTaskCollection->get_Count(&numTasks);
	if (FAILED(hr)) {
		throw nsclient::nsclient_exception("Failed to get count: " + error::com::get(hr));
	}

	for (LONG i = 0; i < numTasks; i++) {
		CComPtr<IRegisteredTask> pRegisteredTask = NULL;
		hr = pTaskCollection->get_Item(_variant_t(i + 1), &pRegisteredTask);
		if (SUCCEEDED(hr)) {
			boost::shared_ptr<tasksched_filter::filter_obj> record(new tasksched_filter::new_filter_obj((IRegisteredTask*)pRegisteredTask, folder));
			modern_filter::match_result ret = filter.match(record);
		}
	}

	BOOST_FOREACH(const std::string f, sub_folders) {
		do_get(taskSched, filter, f, recursive, hidden);
	}
}