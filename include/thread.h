#pragma once

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
	T* pObject_;			// Wrapped object
	HANDLE hStopEvent_;		// Event to signal that the thread has stopped

	typedef struct thread_param {
		Thread* manager;	// The thread manager
		T *instance;		// The thread instance object
		LPVOID lpParam;		// The optional argument to the thread
	} thread_param;

public:
	/**
	 * Default c-tor.
	 * Sets up default values
	 */
	Thread() : hThread_(NULL), dwThreadID_(0), pObject_(NULL), hStopEvent_(NULL) {}
	/**
	 * Default d-tor.
	 * Does not really kill the thread only closes the handle to it.
	 *  @bug Should perhaps kill the thread, delete the object etc ?
	 */
	virtual ~Thread() {
		if (hThread_)
			CloseHandle(hThread_);
		if (hStopEvent_)
			CloseHandle(hStopEvent_);
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
		Thread *manager = param->manager;
		LPVOID lpParam = param->lpParam;
		delete param;

		assert(manager->hStopEvent_);
		DWORD ret = instance->threadProc(lpParam);
		BOOL b = SetEvent(manager->hStopEvent_);
		assert(b);
		return ret;
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
		assert(pObject_ == NULL);
		assert(hStopEvent_ == NULL);
		pObject_ = new T;
		thread_param* param = new thread_param;
		param->instance = pObject_;
		param->manager = this;
		param->lpParam = lpParam;
		hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
		hThread_ = ::CreateThread(NULL,0,threadProc,reinterpret_cast<VOID*>(param),0,&dwThreadID_);
	}
	/**
	 * Ask the thread to terminate (within 5 seconds) if not return false.
	 * @param delay The time to wait for the thread
	 * @return true if the thread has terminated
	 */
	bool exitThread(const unsigned int delay = 5000L) {
		assert(pObject_ != NULL);
		assert(hStopEvent_ != NULL);
		pObject_->exitThread();

		DWORD dwWaitResult = WaitForSingleObject(hStopEvent_, delay);
		switch (dwWaitResult) {
			// The thread got mutex ownership.
			case WAIT_OBJECT_0:
				{
					// @todo pObject should be protected!
					HANDLE hTmp = hStopEvent_;
					T* pTmp = pObject_;
					pObject_ = NULL;
					delete pTmp;
					hStopEvent_ = NULL;
					CloseHandle(hTmp);
				}
				return true;
				// Did not get a signal due to time-out.
			case WAIT_TIMEOUT: 
				return false; 

				// Never got a signal.
			case WAIT_ABANDONED: 
				return false; 
		}
		return false;
	}
	bool hasActiveThread() const {
		// @todo pObject should be protected!
		return pObject_ != NULL;
	}
	const T* getThreadConst() const {
		// @todo pObject should be protected!
		return pObject_;
	}
	T* getThread() const {
		// @todo pObject should be protected!
		return pObject_;
	}
};

