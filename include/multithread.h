#pragma once
#include <thread.h>
#include <process.h>
#include <Mutex.h>

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
	T* pObject_;			// Wrapped object
	HANDLE hStopEvent_;		// Event to signal that the thread has stopped
	HANDLE hMutex_;			// Mutex to protect internal data


	typedef struct thread_param {
		HANDLE hStopEvent;	// The stop event to signal when thread dies
		T *instance;		// The thread instance object
		LPVOID lpParam;		// The optional argument to the thread
		Thread *pCore;
	} thread_param;

public:
	/**
	 * Default c-tor.
	 * Sets up default values
	 */
	Thread() : hThread_(NULL), pObject_(NULL), hStopEvent_(NULL) {
		hMutex_ = CreateMutex(NULL, FALSE, NULL);
		assert(hMutex_ != NULL);
	}
	/**
	 * Default d-tor.
	 * Does not really kill the thread only closes the handle to it.
	 *  @bug Should perhaps kill the thread, delete the object etc ?
	 */
	virtual ~Thread() {
		{
			MutexLock mutex(hMutex_, 5000L);
			if (!mutex.hasMutex()) {
				throw ThreadException("Could not retrieve mutex when killing thread, we are fucked...");
			}
			if (hThread_)
				CloseHandle(hThread_);
			hThread_ = NULL;
			if (hStopEvent_)
				CloseHandle(hStopEvent_);
			hStopEvent_ = NULL;
			delete pObject_;
			pObject_ = NULL;
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
	static void threadProc(LPVOID lpParameter) {
		thread_param* param = static_cast<thread_param*>(lpParameter);
		T* instance = param->instance;
		HANDLE hStopEvent = param->hStopEvent;
		LPVOID lpParam = param->lpParam;
		Thread *pCore = param->pCore;
		delete param;

		if (hStopEvent != NULL) {
			instance->threadProc(lpParam);
			SetEvent(hStopEvent);
		}
		pCore->terminate();
		_endthread();
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
		thread_param* param = NULL;
		{
			MutexLock mutex(hMutex_, 5000L);
			if (!mutex.hasMutex()) {
				throw ThreadException("Could not retrieve mutex, thread not started...");
			}
			if (pObject_) {
				throw ThreadException("Thread already started, thread not started...");
			}
			assert(hStopEvent_ == NULL);
			param = new thread_param;
			param->instance = pObject_ = new T;
			param->hStopEvent = hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
			param->lpParam = lpParam;
			param->pCore = this;
		}
		hThread_ = reinterpret_cast<HANDLE>(::_beginthread(threadProc, 0, reinterpret_cast<VOID*>(param)));
		assert(hThread_ != NULL);
	}
	/**
	 * Ask the thread to terminate (within 5 seconds) if not return false.
	 * @param delay The time to wait for the thread
	 * @return true if the thread has terminated
	 */
	bool exitThread(const unsigned int delay = 5000L) {
		DWORD dwWaitResult = -1;
		{
			MutexLock mutex(hMutex_, 5000L);
			if (!mutex.hasMutex()) {
				throw ThreadException("Could not retrieve mutex, thread not stopped...");
			}
			if (!pObject_)
				return true;
			assert(hStopEvent_ != NULL);
			pObject_->exitThread();
			dwWaitResult = WaitForSingleObject(hStopEvent_, delay);
		}
		switch (dwWaitResult) {
			// The thread got mutex ownership.
			case WAIT_OBJECT_0:
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
		MutexLock mutex(hMutex_, 5000L);
		if (!mutex.hasMutex()) {
			throw ThreadException("Could not retrieve mutex, thread not stopped...");
		}
		return pObject_ != NULL;
	}
	const T* getThreadConst() const {
		MutexLock mutex(hMutex_, 5000L);
		if (!mutex.hasMutex()) {
			throw ThreadException("Could not retrieve mutex, thread not stopped...");
		}
		return pObject_;
	}
	T* getThread() const {
		MutexLock mutex(hMutex_, 5000L);
		if (!mutex.hasMutex()) {
			throw ThreadException("Could not retrieve mutex, thread not stopped...");
		}
		return pObject_;
	}
private:
	void terminate() {
		MutexLock mutex(hMutex_, 5000L);
		if (!mutex.hasMutex()) {
			throw ThreadException("Could not retrieve mutex, thread not stopped...");
		}
		delete pObject_;
		pObject_ = NULL;
		CloseHandle(hStopEvent_);
		hStopEvent_ = NULL;
		hThread_ = NULL;
	}
};

