/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "strEx.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <error.hpp>

class event_exception {
	std::wstring what_;
public:
	event_exception(std::wstring what) : what_(what) {
#ifdef _DEBUG
		std::cout << _T("EventHandler throw an exception: ") << what << std::endl;
#endif

	}
	std::wstring what() { return what_; }
};

/**
* @ingroup NSClient++
* A simple class to wrap the W32 API event object.
*
* @version 1.0
* first version
*
* @date 02-14-2005
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
* @todo 
*
* @bug 
*
*/
class event_handler {
public:
private:
	HANDLE hEvent;
	DWORD dwWaitResult;
	std::wstring name_;
public:
	/**
	* Default c-tor.
	* Creates an unnamed mutex.
	*/
	event_handler(std::wstring name = _T("")) : hEvent(NULL), dwWaitResult(0), name_(name) {
		create();
	}
	event_handler(bool delayed, std::wstring name) : hEvent(NULL), dwWaitResult(0), name_(name) {
		if (!delayed) {
			create();
		}
	}
	/**
	* Default d-tor.
	* Destroys releases the mutex handle.
	*/
	virtual ~event_handler() {
		close();
	}
	void close() {
		if (hEvent)
			CloseHandle(hEvent);
		hEvent = NULL;
	}
	void create() {
		if (hEvent)
			CloseHandle(hEvent);
		SECURITY_DESCRIPTOR sd = { 0 };
		::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
		::SetSecurityDescriptorDacl(&sd, TRUE, 0, FALSE);
		SECURITY_ATTRIBUTES sa = { 0 };
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = &sd;
		hEvent = CreateEvent(&sa,false,false,name_.empty()?NULL:name_.c_str());
		if (hEvent == NULL)
			throw event_exception(_T("Failed to create ") + name_ + _T(" event: ") + error::lookup::last_error());
	}
	void open() {
		if (hEvent)
			CloseHandle(hEvent);
		hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE,name_.empty()?NULL:name_.c_str());
		if (hEvent == NULL)
			throw event_exception(_T("Failed to create ") + name_ + _T(" event: ") + error::lookup::last_error());
	}
	void create(std::wstring name) {
		name_ = name;
		create();
	}
	/**
	* HANDLe cast operator to retrieve the handle from the enclosed mutex object.
	* @return a handle to the mutex object.
	*/
	operator HANDLE () const {
		return hEvent;
	}
	/**
	* Release the mutex
	*/
	void set() {
		if (hEvent == NULL)
			throw event_exception(_T("Failed to set event ") + name_ + _T("(mutex handle is null)"));
		if (!SetEvent(hEvent))
			throw event_exception(_T("Failed to set event ") + name_ + _T(": ") + error::lookup::last_error());
	}
	/**
	* Waits for the mutex object.
	* @timeout The timeout before abandoning wait
	*/
	bool accuire(DWORD timeout = 5000L) {
		if (hEvent == NULL)
			throw event_exception(_T("Failed to accuire event '") + name_ + _T("' (mutex handle is null)"));
		dwWaitResult = WaitForSingleObject(hEvent, timeout);
		switch (dwWaitResult) {
			// The thread got mutex ownership.
		case WAIT_OBJECT_0:
			return true;
		case WAIT_TIMEOUT: 
			return false;
		case WAIT_ABANDONED: 
			return true;
		default:
			throw event_exception(_T("Unknown returncode from <") + name_ + _T(">event.aqquire: ") + get_wait_result());

		}
	}
	inline std::wstring get_wait_result() {
		switch (dwWaitResult) {
		case WAIT_OBJECT_0:
			return _T("found");
		case WAIT_TIMEOUT: 
			return _T("timed out");
		case WAIT_ABANDONED: 
			return _T("abandoned");
		case WAIT_FAILED:
			return _T("failed: ") + error::lookup::last_error();
		default:
			return _T("unknown: ") + strEx::itos(dwWaitResult);
		}
	}
	/**
	* Get the result of the wait operation.
	* @return Result of the wait operation
	*/
	DWORD getWaitResult() const {
		return dwWaitResult;
	}

	bool isOpen() const {
		return hEvent != NULL;
	}


	static bool exists(std::wstring name) {
		HANDLE hTmpEvent = OpenEvent(EVENT_ALL_ACCESS, false, name.c_str());
		if (hTmpEvent == NULL) {
			return false;
		}
		CloseHandle(hTmpEvent);
		return true;
	}

	std::wstring get_name() const {
		return name_;
	}

};
