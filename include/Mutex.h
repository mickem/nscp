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

#include "Singleton.h"
#include "strEx.h"
#include <windows.h>
#include <iostream>
#include <error.hpp>

class mutex_exception {
	std::wstring what_;
public:
	mutex_exception(std::wstring what) : what_(what) {}
	std::wstring what() { return what_; }
};

/**
 * @ingroup NSClient++
 * A simple class to wrap the W32 API mutex object.
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
class MutexHandler {
public:
private:
	HANDLE hMutex;
	DWORD dwWaitResult;
	bool bCreated;
public:
	/**
	 * Default c-tor.
	 * Creates an unnamed mutex.
	 */
	MutexHandler(std::wstring name = _T("")) : hMutex(NULL), dwWaitResult(0), bCreated(false) {
		//std::wcout << _T("Creating mutex: ") << name << std::endl;
		hMutex = CreateMutex(NULL, FALSE, name.empty()?NULL:name.c_str());
		if (hMutex == NULL && GetLastError() == ERROR_ALREADY_EXISTS )
			hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, name.empty()?NULL:name.c_str());
		else 
			bCreated = true;
		if (hMutex == NULL)
			throw mutex_exception(_T("Failed to create mutex: ") + error::lookup::last_error());
	}
	/**
	 * Default d-tor.
	 * Destroys releases the mutex handle.
	 */
	virtual ~MutexHandler() {
		if (hMutex)
			CloseHandle(hMutex);
		hMutex = NULL;
	}
	void close() {
		if (hMutex)
			CloseHandle(hMutex);
		hMutex = NULL;
	}
	bool mutexWasCreated() {
		return bCreated;
	}
	/**
	 * HANDLe cast operator to retrieve the handle from the enclosed mutex object.
	 * @return a handle to the mutex object.
	 */
	operator HANDLE () const {
		return hMutex;
	}
	/**
	* Release the mutex
	*/
	void releaseLock() {
		if (hMutex == NULL)
			throw mutex_exception(_T("Failed to release mutex lock (mutex handle is null)"));
		if (!ReleaseMutex(hMutex))
			throw mutex_exception(_T("Failed to relase the mutex: ") + error::lookup::last_error());
	}
	/**
	* Waits for the mutex object.
	* @timeout The timeout before abandoning wait
	*/
	bool accuireLock(DWORD timeout = 5000L) {
		if (hMutex == NULL)
			throw mutex_exception(_T("Failed to get mutex lock (mutex handle is null)"));
		dwWaitResult = WaitForSingleObject(hMutex, timeout);
		switch (dwWaitResult) {
			// The thread got mutex ownership.
		case WAIT_OBJECT_0:
			return true;
		case WAIT_TIMEOUT: 
			return false;
		case WAIT_ABANDONED: 
			return true;
		default:
			throw mutex_exception(_T("Unknown returncode from the mutex: ") + strEx::itos(dwWaitResult));
			
		}
	}
	/**
	* Get the result of the wait operation.
	* @return Result of the wait operation
	*/
	DWORD getWaitResult() const {
		return dwWaitResult;
	}
};

/**
 * @ingroup NSClient++
 * A simple mutex-lock class that makes mutex management simple.
 * By using the c-tor/d-tor to lock and release the mutex a block only needs to "contain" this object to be "safe".
 *<pre>
 * {
 *   MutexLock mutex(mutexHandler);
 *   if (!mutex.hasMutex()) {
 *     // Do stuff thread safe...
 *   }
 * }
 *</pre>
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
 */
class MutexLock {
private:
	bool bHasMutex;		// Status: true if we have a mutex lock
	HANDLE hMutex_;		// Handle to the mutex object.
	DWORD dwWaitResult;	// Result from the mutex operations
public:
	/**
	 * Default c-tor.
	 * Waits for the mutex object.
	 * @param hMutex The mutex to use
	 * @timeout The timeout before abandoning wait
	 */
	MutexLock(HANDLE hMutex, DWORD timeout = 5000L) : bHasMutex(false), hMutex_(hMutex) {
		if (hMutex_ == NULL) {
			throw mutex_exception(_T("Failed to get mutex lock (mutex handle is null)"));
			/*
			bHasMutex = false;
			return;
			*/
		}
		dwWaitResult = WaitForSingleObject(hMutex_, timeout);
		switch (dwWaitResult) {
			// The thread got mutex ownership.
        case WAIT_OBJECT_0:
			bHasMutex = true;
			break;
        case WAIT_TIMEOUT:
			bHasMutex = false;
			break;
        case WAIT_ABANDONED:
			bHasMutex = false;
			break;
		}
	}
	/**
	 * An attempt to simplify the has mutex thingy (don't know if it works, haven't tried it since I wrote this class a few years ago :)
	 * @return true if we have a mutex lock.
	 */
	operator bool () const {
		return bHasMutex;
	}
	/**
	 * Check if we actually got the mutex (might have timed out)
	 * @return 
	 */
	bool hasMutex() const {
		return bHasMutex;
	}
	/**
	 * Default d-tor.
	 * Release the mutex
	 */
	virtual ~MutexLock() {
		if (bHasMutex)
	        ReleaseMutex(hMutex_);
		bHasMutex = false;
	}
	/**
	 * Get the result of the wait operation.
	 * @return Result of the wait operation
	 */
	DWORD getWaitResult() const {
		return dwWaitResult;
	}
};

class ManualMutexLock {
private:
	bool bHasMutex;		// Status: true if we have a mutex lock
	HANDLE hMutex_;		// Handle to the mutex object.
	DWORD dwWaitResult;	// Result from the mutex operations
public:
	/**
	* Default c-tor.
	* Waits for the mutex object.
	* @param hMutex The mutex to use
	* @timeout The timeout before abandoning wait
	*/
	ManualMutexLock(HANDLE hMutex) : bHasMutex(false), hMutex_(hMutex) {}
	
	/**
	* Waits for the mutex object.
	* @timeout The timeout before abandoning wait
	*/
	void lock(DWORD timeout = 5000L) {
		if (hMutex_ == NULL) {
			throw mutex_exception(_T("Failed to get mutex lock (mutex handle is null)"));
			/*
			std::wcout << _T("Error in mutex lock: ") << std::endl;
			bHasMutex = false;
			return;
			*/
		}
		dwWaitResult = WaitForSingleObject(hMutex_, timeout);
		switch (dwWaitResult) {
			// The thread got mutex ownership.
		case WAIT_OBJECT_0:
			bHasMutex = true;
			break;
		case WAIT_TIMEOUT: 
			bHasMutex = false;
			break;
		case WAIT_ABANDONED: 
			bHasMutex = false;
			break;
		}
	}
	/**
	* An attempt to simplify the has mutex thingy (don't know if it works, haven't tried it since I wrote this class a few years ago :)
	* @return true if we have a mutex lock.
	*/
	operator bool () const {
		return bHasMutex;
	}
	/**
	* Check if we actually got the mutex (might have timed out)
	* @return 
	*/
	bool hasMutex() const {
		return bHasMutex;
	}
	/**
	* Default d-tor.
	* Release the mutex
	*/
	virtual ~ManualMutexLock() {
		if (bHasMutex)
			ReleaseMutex(hMutex_);
		bHasMutex = false;
	}
	/**
	* Release the mutex
	*/
	void release() {
		if (bHasMutex)
			ReleaseMutex(hMutex_);
		bHasMutex = false;
	}
	/**
	* Get the result of the wait operation.
	* @return Result of the wait operation
	*/
	DWORD getWaitResult() const {
		return dwWaitResult;
	}
};
