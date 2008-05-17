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

#include <process.h>
#include <Mutex.h>

class ThreadException {
public:
	std::wstring e_;
	ThreadException(std::wstring e) : e_(e) {
		std::wcerr << e << std::endl;
	}
};

/**
 * @ingroup NSClientCompat
 * Thread helper class.
 * This class wraps another class inside it and makes that class run in the background as a working thread.
 * Notice that no threading issues are handled so you are on your own when it comes to that. 
 * This simply wraps some API function inside a pretty interface.
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
 * @todo Make this class (or another) handle multiple instances of the thread ?
 */
template <class T> 
class Thread {
private:
	std::wstring threadid_;
	HANDLE hThread_;		// Thread handle
	T* pObject_;			// Wrapped object
	HANDLE hMutex_;			// Mutex to protect internal data
	unsigned uThreadID;		// THe thread ID


	bool bThreadHasTerminated;
	bool bThreadHasBeenClosed;

#define THREAD_MAGIC_ID 123456

	typedef struct thread_param {
		T *instance;		// The thread instance object
		LPVOID lpParam;		// The optional argument to the thread
		Thread *pCore;
		unsigned int magic_id;
	} thread_param;

public:
	/**
	 * Default c-tor.
	 * Sets up default values
	 */
	Thread(std::wstring threadid) : threadid_(threadid), hThread_(NULL), pObject_(NULL), uThreadID(-1), bThreadHasTerminated(false), bThreadHasBeenClosed(false) {
		hMutex_ = CreateMutex(NULL, FALSE, NULL);
		if (hMutex_ == NULL) {
			std::wcerr << _T("Failed to create thread mutec for thread: ") << threadid << _T(": ") << GetLastError() << std::endl;
		}
	}
	/**
	 * Default d-tor.
	 * Does not really kill the thread only closes the handle to it.
	 *  @bug Should perhaps kill the thread, delete the object etc ?
	 */
	virtual ~Thread() {
		{
			if (bThreadHasBeenClosed && bThreadHasTerminated)
				;
			else if (bThreadHasBeenClosed||bThreadHasTerminated)
				std::wcout << "Thread has not terminated correctly: " << threadid_ << "..." << std::endl;
			/*
			MutexLock mutex(hMutex_, 5000L);
			if (!mutex.hasMutex()) {
				throw ThreadException("Could not retrieve mutex when killing thread, we are fucked...");
			}
			hThread_ = NULL;
			if (hStopEvent_)
				CloseHandle(hStopEvent_);
			hStopEvent_ = NULL;
			delete pObject_;
			*/
//			pObject_ = NULL;
//			CloseHandle(hThread_);
		}
		if (hMutex_)
			CloseHandle(hMutex_);
		hMutex_ = NULL;
	}

private:
	/**
	 * Static thread instance (this is the real thread procedure that wraps the internal objects)
	 * This function will wait for <b>nice</b> termination of the process so if that does not happen the thread canoot quit.
	 *
	 * @param lpParameter thread_param* with arguments for the thread construction
	 * @return exit status
	 */
	static unsigned __stdcall threadProc(void* lpParameter) {
		thread_param* param = static_cast<thread_param*>(lpParameter);
		if (param->magic_id != THREAD_MAGIC_ID) {
			std::wcerr << _T("Thread magic ID check failed :(") << std::endl;
			_endthreadex( 0 );
			return 0;
		}
		T* instance = param->instance;
		LPVOID lpParam = param->lpParam;
		Thread *pCore = param->pCore;
		delete param;
		unsigned returnCode = 0;
		try {
			returnCode = instance->threadProc(lpParam);
		} catch (...) {
			std::wcerr << _T("Unhandled exception in thread a :(") << std::endl;
		}
		pCore->terminate();
		_endthreadex( 0 );
		return returnCode;
	}

public:
	/**
	 * Create a thread and possibly send a parameter to it.
	 * If a thread has already been create this function will throw an exception
	 * <b>NOTICE</b> The object returned is managed inside this object so it is not very safe to use it.
	 *
	 * @param lpParam An argument to the thread
	 * @return An instance of the thread object.
	 * @bug the object return thing is *unsafe* and should be changed (if the thread is terminated that pointer is invalidated without any signal).
	 */
	void createThread(LPVOID lpParam = NULL) {
		if (hMutex_ == NULL)
			throw ThreadException(_T("No mutex in createThread (") + threadid_ + _T(") not started..."));
		thread_param* param = NULL;
		{
			MutexLock mutex(hMutex_, 5000L);
			if (!mutex.hasMutex()) {
				throw ThreadException(_T("Could not retrieve mutex, thread (") + threadid_ + _T(") not started..."));
			}
			if (pObject_) {
				throw ThreadException(_T("Thread already started, thread (") + threadid_ + _T(") not started..."));
			}
			param = new thread_param;
			if (param == NULL)
				throw ThreadException(_T("Failed to allocate memory for new thread: ") + threadid_);
			param->instance = pObject_ = new T;
			if (param->instance == NULL)
				throw ThreadException(_T("Failed to allocate memory for new thread: ") + threadid_);
			param->lpParam = lpParam;
			param->pCore = this;
			param->magic_id = THREAD_MAGIC_ID;
		}
		uintptr_t thread_handle = ::_beginthreadex(NULL, 0, threadProc, reinterpret_cast<VOID*>(param), 0, &uThreadID);
		if (thread_handle == 0 || thread_handle == 1 || thread_handle == -1) {
			throw ThreadException(_T("Failed to create the thread (") + threadid_ + _T(")."));
		}
		hThread_ = reinterpret_cast<HANDLE>(thread_handle);
	}
	/**
	 * Ask the thread to terminate (within 5 seconds) if not return false.
	 * @param delay The time to wait for the thread
	 * @return true if the thread has terminated
	 */
	bool exitThread(const unsigned int delay = 20000L) {
		if (hMutex_ == NULL)
			throw ThreadException(_T("No mutex in createThread (") + threadid_ + _T(") not started..."));
		DWORD dwWaitResult = -1;
		{
			MutexLock mutex(hMutex_, delay);
			if (!mutex.hasMutex()) {
				throw ThreadException(_T("Could not retrieve mutex, thread (") + threadid_ + _T(") not stopped..."));
			}
			if (!pObject_)
				return true;
			pObject_->exitThread();
		}
		dwWaitResult = WaitForSingleObject(hThread_, delay);
		if (dwWaitResult == WAIT_OBJECT_0) {
			bThreadHasBeenClosed = true;
			CloseHandle(hThread_);
			delete pObject_;
			pObject_ = NULL;
			return true;
		}
		std::wcerr << _T("Failed to terminate thread: ") << threadid_ << _T("...") << std::endl;
		return false;
	}
	bool hasActiveThread() const {
		MutexLock mutex(hMutex_, 5000L);
		if (!mutex.hasMutex()) {
			throw ThreadException(_T("Could not retrieve mutex, thread (") + threadid_ + _T(") not stopped..."));
		}
		if (bThreadHasTerminated||bThreadHasBeenClosed)
			return false;
		return pObject_ != NULL;
	}
	const T* getThreadConst() const {
		MutexLock mutex(hMutex_, 5000L);
		if (!mutex.hasMutex()) {
			throw ThreadException(_T("Could not retrieve mutex, thread (") + threadid_ + _T(") not stopped..."));
		}
		if (bThreadHasTerminated||bThreadHasBeenClosed)
			return NULL;
		return pObject_;
	}
	T* getThread() {
		MutexLock mutex(hMutex_, 5000L);
		if (!mutex.hasMutex()) {
			throw ThreadException(_T("Could not retrieve mutex, thread (") + threadid_ + _T(") not stopped..."));
		}
		if (bThreadHasTerminated||bThreadHasBeenClosed)
			return NULL;
		return pObject_;
	}
private:
	void terminate() {
		bThreadHasTerminated = true;
	}
};

