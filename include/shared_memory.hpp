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

#include "strEx.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <error.hpp>

class shared_memory_exception {
	std::wstring what_;
public:
	shared_memory_exception(std::wstring what) : what_(what) {
#ifdef _DEBUG
		std::wcout << _T("SharedMemorHandler throw an exception: ") << what << std::endl;
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
class shared_memory_handler {
public:
private:
	HANDLE hMem;
	DWORD dwWaitResult;
	LPTSTR pBuffer;
	std::wstring name_;
	DWORD dwSize_;
public:
	/**
	* Default c-tor.
	* Creates an unnamed mutex.
	*/
	shared_memory_handler(std::wstring name = _T(""), DWORD dwSize = 4096) : hMem(NULL), name_(name), pBuffer(NULL), dwSize_(dwSize) {
		create();
	}
	shared_memory_handler(bool delayed, std::wstring name, DWORD dwSize = 4096) : hMem(NULL), name_(name), dwSize_(dwSize) {
		if (!delayed) {
			create();
		}
	}
	/**
	* Default d-tor.
	* Destroys releases the mutex handle.
	*/
	virtual ~shared_memory_handler() {
		close();
	}
	void close() {
		if (hMem)
			CloseHandle(hMem);
		hMem=NULL;
		if (pBuffer)
			UnmapViewOfFile(pBuffer);
		pBuffer=NULL;
	}
	void open() {
		hMem = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name_.c_str());
		if (hMem == NULL || hMem == INVALID_HANDLE_VALUE)	{
			throw shared_memory_exception(_T("Error occured while creating file mapping object '") + name_ + _T("' :") + error::lookup::last_error());
		}

		pBuffer = (LPTSTR) MapViewOfFile(hMem, // handle to map object
			FILE_MAP_ALL_ACCESS, // read/write permission
			0,
			0,
			dwSize_);

		if (pBuffer == NULL) {
			CloseHandle(hMem);
			throw shared_memory_exception(_T("Error occured while mapping view of the file '") + name_ + _T("' :") + error::lookup::last_error());
		}
	}	void create() {
		SECURITY_DESCRIPTOR sd = { 0 };
		if (!::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
			throw shared_memory_exception(_T("Error occured while initializing security descriptor for '") + name_ + _T("' :") + error::lookup::last_error());
		}
		if (!::SetSecurityDescriptorDacl(&sd, TRUE, 0, FALSE)) {
			throw shared_memory_exception(_T("Error occured while setting security descriptor for '") + name_ + _T("' :") + error::lookup::last_error());
		}
		SECURITY_ATTRIBUTES sa = { 0 };
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = &sd;
		hMem = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, dwSize_, name_.c_str());
		if (hMem == NULL || hMem == INVALID_HANDLE_VALUE)	{
			throw shared_memory_exception(_T("Error occured while creating file mapping object '") + name_ + _T("' :") + error::lookup::last_error());
		}

		pBuffer = (LPTSTR) MapViewOfFile(hMem, // handle to map object
			FILE_MAP_ALL_ACCESS, // read/write permission
			0,
			0,
			dwSize_);

		if (pBuffer == NULL) {
			CloseHandle(hMem);
			throw shared_memory_exception(_T("Error occured while mapping view of the file '") + name_ + _T("' :") + error::lookup::last_error());
		}
	}
	void create(std::wstring name) {
		name_ = name;
		create();
	}
	/**
	* HANDLe cast operator to retrieve the handle from the enclosed mutex object.
	* @return a handle to the mutex object.
	*/
	operator LPCTSTR () const {
		return pBuffer;
	}
	LPTSTR getBufferUnsafe() const {
		return pBuffer;
	}
	bool isOpen() {
		return hMem != NULL && pBuffer != NULL;
	}
	std::wstring get_name() const {
		return name_;
	}
};
