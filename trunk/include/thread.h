// Thread.h: interface for the Thread class.
//
//////////////////////////////////////////////////////////////////////

#pragma once


/**
 * @ingroup NSClientCompat
 * Simple unnamed mutex to handle thread exit wait (used by the Thread class below)
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
 */
class UnnamedMutex {
private:
	HANDLE hMutex_;
public:
	/**
	 * Default c-tor.
	 * Creates an unnamed mutex with a given owner status
	 * @param owner true if we want to become owner of the mutex
	 */
	UnnamedMutex(bool owner = false) {
		hMutex_ = ::CreateMutex(NULL, owner, NULL);
	}
	/**
	 * Default d-tor.
	 * Closes the mutex
	 */
	virtual ~UnnamedMutex() {
		CloseHandle(hMutex_);
	}
	/**
	 * Wait for the mutex or timeout in dwMilliseconds milliseconds.
	 * @param dwMilliseconds The timeout value
	 * @return status
	 */
	DWORD wait(DWORD dwMilliseconds = 0L) {
		return ::WaitForSingleObject(hMutex_, dwMilliseconds);
	}
	/**
	 * Release the mutex
	 * @return status
	 */
	BOOL free() {
		return ::ReleaseMutex(hMutex_);
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
	HANDLE hThread_;		// Thread handle
	DWORD dwThreadID_;		// Thread ID
	UnnamedMutex endMutext;	// mutex to wait for end of thread
	T* pObject_;			// Wrapped object

	typedef struct thread_param {
		Thread* core;
		T *instance;
		LPVOID lpParam;
	} thread_param;

public:
	/**
	 * Default c-tor.
	 * Sets up default values
	 */
	Thread() : endMutext(false), hThread_(NULL), dwThreadID_(0), pObject_(NULL) {}
	/**
	 * Default d-tor.
	 * Does not really kill the thread only closes the handle to it.
	 *  @bug Should perhaps kill the thread, delete the object etc ?
	 */
	virtual ~Thread() {
		if (hThread_)
			CloseHandle(hThread_);
	}

private:
	/**
	 * Static thread instance (this is the real thread procedure that wraps the internal objects)
	 * This function will wait for <b>nice</b> termination of the process so if that does not happen the thread canoot quit.
	 *
	 * @param lpParameter thread_param* with arguments for the thread construction
	 * @return exit status
	 */
	static DWORD WINAPI threadProc(LPVOID lpParameter) {
		thread_param* param = static_cast<thread_param*>(lpParameter);
		T* instance = param->instance;
		Thread *core = param->core;
		LPVOID lpParam = param->lpParam;
		delete param;
		core->endMutext.wait();
		DWORD ret = instance->threadProc(lpParam);
		core->endMutext.free();
		return 0;
	}

public:
	/**
	 * Create a thread and possibly send a parameter to it.
	 * If a thread has already been create this function will throw an exception
	 * <b>NOTICE</b> The object returned is managed inside this object so it is not very safe to use it.
	 *
	 * @param lpParam An argument to the thread
	 * @return An instance of the thread object.
	 * @throws char
	 * @bug the object return thing is *unsafe* and should be changed (if the thread is terminated that pointer is invalidated without any signal).
	 */
	T* createThread(LPVOID lpParam = NULL) {
		if (pObject_)
			throw "Could not create thread";
		pObject_ = new T;
		thread_param* param = new thread_param;
		param->instance = pObject_;
		param->core = this;
		param->lpParam = lpParam;
		hThread_ = ::CreateThread(NULL,0,threadProc,reinterpret_cast<VOID*>(param),0,&dwThreadID_);
		return pObject_;
	}
	/**
	 * Ask the thread to terminate (within 5 seconds) if not return false.
	 * @return true if the thread has terminated
	 */
	bool exitThread() {
		if (!pObject_)
			throw "Could not terminate thread, has not been started yet...";
		pObject_->exitThread();
		std::cout << "Waiting for socket to close..." << std::endl;
		DWORD dwWaitResult = endMutext.wait(5000L);
		switch (dwWaitResult) {
			// The thread got mutex ownership.
			case WAIT_OBJECT_0:
				// TODO: Potential race condition if multipåle threads try to terminate the thread...
				delete pObject_;
				pObject_ = NULL;
				endMutext.free();
				return true;
				// Cannot get mutex ownership due to time-out.
			case WAIT_TIMEOUT: 
				return false; 

				// Got ownership of the abandoned mutex object.
			case WAIT_ABANDONED: 
				return false; 
		}
		return false;
	}
};

