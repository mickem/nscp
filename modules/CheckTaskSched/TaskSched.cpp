// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "TaskSched.h"

#include <Mstask.h>
#include <atlbase.h>
#include <comdef.h>
#include <objidl.h>
#include <taskschd.h>

#include <error/error_com.hpp>
#include <map>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <win/com_helpers.hpp>

#include "task_facts.hpp"

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")

#define TASKS_TO_RETRIEVE 5

void find_old(const TaskSched::visitor &consume, const bool strict) {
  CComPtr<ITaskScheduler> taskSched;
  HRESULT hr = CoCreateInstance(CLSID_CTaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskScheduler, reinterpret_cast<void **>(&taskSched));
  if (FAILED(hr)) {
    throw nsclient::nsclient_exception("CoCreateInstance for CLSID_CTaskScheduler failed: " + error::com::get(hr));
  }

  CComPtr<IEnumWorkItems> taskSchedEnum;
  hr = taskSched->Enum(&taskSchedEnum);
  if (FAILED(hr)) {
    throw nsclient::nsclient_exception("Failed to enum work items: " + error::com::get(hr));
  }

  LPWSTR *lpwszNames = nullptr;
  DWORD dwFetchedTasks = 0;
  while (SUCCEEDED(hr = taskSchedEnum->Next(TASKS_TO_RETRIEVE, &lpwszNames, &dwFetchedTasks)) && (dwFetchedTasks != 0)) {
    // Release the enumerator's allocation before invoking a visitor that may
    // throw. The COM task references themselves are scoped below.
    std::vector<std::wstring> names;
    for (DWORD i = 0; i < dwFetchedTasks; ++i) {
      names.emplace_back(lpwszNames[i]);
      CoTaskMemFree(lpwszNames[i]);
    }
    CoTaskMemFree(lpwszNames);
    for (const auto &name : names) {
      CComPtr<ITask> task;
      std::string title = utf8::cvt<std::string>(name);
      const HRESULT activated = taskSched->Activate(name.c_str(), IID_ITask, reinterpret_cast<IUnknown **>(&task));
      if (FAILED(activated) || !task) {
        if (strict) throw nsclient::nsclient_exception("Failed to read task " + title + ": " + error::com::get(activated));
        continue;
      }
      std::shared_ptr<tasksched_filter::filter_obj> record(new tasksched_filter::old_filter_obj((ITask *)task, title));
      consume(record);
    }
  }
  if (strict && FAILED(hr)) throw nsclient::nsclient_exception("Failed to enumerate tasks: " + error::com::get(hr));
}

void do_get(CComPtr<ITaskService> taskSched, const TaskSched::visitor &consume, std::string folder, bool recursive, bool hidden, bool strict);

void TaskSched::findAll(tasksched_filter::filter &filter, std::string computer, std::string user, std::string domain, std::string password, std::string folder,
                        bool recursive, bool hidden, bool old) {
  visit([&filter](const std::shared_ptr<tasksched_filter::filter_obj> &record) { filter.match(record); }, computer, user, domain, password, folder, recursive,
        hidden, old);
}

void TaskSched::visit(const visitor &consume, std::string computer, std::string user, std::string domain, std::string password, std::string folder,
                      bool recursive, bool hidden, bool old, bool strict) {
  if (old) {
    return find_old(consume, strict);
  }
  CComPtr<ITaskService> taskSched;
  HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void **>(&taskSched));
  if (FAILED(hr)) {
    if (strict && hr != REGDB_E_CLASSNOTREG) throw nsclient::nsclient_exception("Failed to create task service: " + error::com::get(hr));
    NSC_DEBUG_MSG("Failed to create mordern finder using old method: " + error::com::get(hr));
    return find_old(consume, strict);
  }
  _variant_t vComputer;
  if (!computer.empty()) vComputer = utf8::cvt<std::wstring>(computer).c_str();
  _variant_t vUser;
  if (!user.empty()) vUser = utf8::cvt<std::wstring>(user).c_str();
  _variant_t vDomain;
  if (!domain.empty()) vDomain = utf8::cvt<std::wstring>(domain).c_str();
  _variant_t vPassword;
  if (password.empty()) vPassword = utf8::cvt<std::wstring>(password).c_str();
  hr = taskSched->Connect(vComputer, vUser, vDomain, vPassword);

  if (FAILED(hr)) {
    NSC_DEBUG_MSG("Failed to connect to: computer: '" + computer + "', domain: '" + domain + "', user: '" + user + "', password: '" +
                  std::string(password.size(), '*') + "': " + str::xtos(hr));
    throw nsclient::nsclient_exception("Failed to connect to task service on " + computer + ": " + error::com::get(hr));
  }
  do_get(taskSched, consume, folder, recursive, hidden, strict);
}

void do_get(CComPtr<ITaskService> taskSched, const TaskSched::visitor &consume, std::string folder, bool recursive, bool hidden, bool strict) {
  CComPtr<ITaskFolder> pRootFolder;
  HRESULT hr = taskSched->GetFolder(_bstr_t(utf8::cvt<std::wstring>(folder).c_str()), &pRootFolder);
  if (FAILED(hr)) {
    throw nsclient::nsclient_exception("Failed to get root folder " + folder + ": " + error::com::get(hr));
  }

  std::vector<std::string> sub_folders;
  if (recursive) {
    CComPtr<ITaskFolderCollection> folders;
    if (FAILED(hr = pRootFolder->GetFolders(0, &folders)))
      throw nsclient::nsclient_exception("Failed to get folders below " + folder + ": " + error::com::get(hr));
    LONG count = 0;
    if (FAILED(hr = folders->get_Count(&count)))
      throw nsclient::nsclient_exception("Failed to get count of folders below " + folder + ": " + error::com::get(hr));
    for (LONG i = 0; i < count; ++i) {
      CComPtr<ITaskFolder> inst;
      if (FAILED(hr = folders->get_Item(_variant_t(i + 1), &inst)))
        throw nsclient::nsclient_exception("Failed to get folder item " + str::xtos(i) + ": " + error::com::get(hr));
      BSTR str;
      if (FAILED(hr = inst->get_Path(&str))) throw nsclient::nsclient_exception("Failed to get path for " + str::xtos(i) + ": " + error::com::get(hr));
      _bstr_t sstr(str, FALSE);
      sub_folders.push_back(utf8::cvt<std::string>(std::wstring(sstr)));
    }
  }

  CComPtr<IRegisteredTaskCollection> pTaskCollection;
  hr = pRootFolder->GetTasks(hidden ? TASK_ENUM_HIDDEN : NULL, &pTaskCollection);
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
      std::shared_ptr<tasksched_filter::filter_obj> record(new tasksched_filter::new_filter_obj((IRegisteredTask *)pRegisteredTask, folder));
      consume(record);
    } else if (strict) {
      throw nsclient::nsclient_exception("Failed to read task in " + folder + ": " + error::com::get(hr));
    }
  }

  for (const std::string f : sub_folders) {
    do_get(taskSched, consume, f, recursive, hidden, strict);
  }
}

std::vector<task_facts::task> task_facts::gather() {
  const com_helper::mta_scope com;
  if (!com.is_ready()) throw nsclient::nsclient_exception("Failed to initialize COM: " + error::com::get(com.result()));
  std::vector<task> tasks;
  TaskSched query;
  query.visit(
      [&tasks](const std::shared_ptr<tasksched_filter::filter_obj> &row) {
        task t;
        t.name = row->get_title();
        t.folder = row->is_new() ? row->get_folder() : "\\";
        t.id = row->is_new() ? row->get_uri() : "\\" + t.name;
        // Legacy GetStatus is a runtime status, not a bitmask. Its enabled
        // setting is TASK_FLAG_DISABLED in GetFlags.
        t.enabled = row->is_new() ? row->is_enabled() != 0 : (row->get_flags() & TASK_FLAG_DISABLED) == 0;
        if (row->is_new()) t.hidden = row->get_hidden() ? 1 : 0;
        tasks.push_back(t);
      },
      "", "", "", "", "\\", true, true, false, true);
  return tasks;
}
