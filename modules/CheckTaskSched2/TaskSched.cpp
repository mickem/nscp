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
#include <comdef.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")

void TaskSched::findAll(tasksched_filter::filter_result result, tasksched_filter::filter_argument args, tasksched_filter::filter_engine engine) {
	CComPtr<ITaskService> taskSched;
	HRESULT hr = CoCreateInstance( CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void**>(&taskSched));
	if (FAILED(hr)) {
		throw Exception(_T("CoCreateInstance for CLSID_CTaskScheduler failed!"), hr);
	}

	taskSched->Connect(_variant_t(), _variant_t(),
		_variant_t(), _variant_t());

	ITaskFolder *pRootFolder = NULL;
	hr = taskSched->GetFolder( _bstr_t( L"\\") , &pRootFolder );
	if (FAILED(hr)) {
		throw Exception(_T("Failed to get root folder!"), hr);
	}

	CComPtr<IRegisteredTaskCollection> pTaskCollection;
	hr = pRootFolder->GetTasks(NULL, &pTaskCollection);
	if (FAILED(hr)) {
		throw Exception(_T("Failed to enum work items failed!"), hr);
	}

	LONG numTasks = 0;
	hr = pTaskCollection->get_Count(&numTasks);
	if (FAILED(hr)) {
		throw Exception(_T("Failed to get count!"), hr);
	}

	if( numTasks == 0 ) {
		return;
	}

	TASK_STATE taskState;

	for(LONG i=0; i < numTasks; i++) {
		CComPtr<IRegisteredTask> pRegisteredTask = NULL;
		hr = pTaskCollection->get_Item(_variant_t(i+1), &pRegisteredTask);
		if(SUCCEEDED(hr)) {
			tasksched_filter::filter_obj info((IRegisteredTask*)pRegisteredTask);
			result->process(info, engine->match(info));
		}
	}
}
