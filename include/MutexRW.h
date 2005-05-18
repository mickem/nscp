#pragma once

#include <iostream>
#include <assert.h>

//#define TRACE(x, y) 
//#define VERIFY(x) x

class MutexRW
{
protected:
	HANDLE		m_semReaders;
	HANDLE		m_semWriters;
	int			m_nReaders;
public:
	MutexRW() 
		: m_semReaders(NULL),
		m_semWriters(NULL),
		m_nReaders(0)
	{
		// initialize the Readers & Writers variables
		m_semReaders	= ::CreateSemaphore(NULL, 1, 1, NULL);
		m_semWriters	= ::CreateSemaphore(NULL, 1, 1, NULL);
		m_nReaders		= 0;

		if (m_semReaders == NULL || m_semWriters == NULL)
		{
			LPVOID lpMsgBuf;
			FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				GetLastError(),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &lpMsgBuf,
				0,
				NULL 
				);
			std::cout << "***** ERROR: CreateSemaphore: %s\n" << (LPCTSTR)lpMsgBuf;
			//			  TRACE( "***** ERROR: CreateSemaphore: %s\n", (LPCTSTR)lpMsgBuf );
			LocalFree( lpMsgBuf );			
		}
	};

	virtual ~MutexRW()
	{
		if (m_semWriters)
			::CloseHandle(m_semWriters);

		m_semWriters = NULL;
		if (m_semReaders)
			::CloseHandle(m_semReaders);
		m_semReaders = NULL;
	}

	inline bool Lock_DataRead(DWORD dwMilliseconds = INFINITE) {
		DWORD dwEvent = WAIT_TIMEOUT;

		// P( semReaders )
		dwEvent = ::WaitForSingleObject( m_semReaders, dwMilliseconds );
		if (dwEvent != WAIT_OBJECT_0)
			return false;
		//		  assert(dwEvent == WAIT_OBJECT_0);

		m_nReaders++;

		if (m_nReaders == 1)
		{
			// P( semWriters )
			dwEvent = ::WaitForSingleObject( m_semWriters, dwMilliseconds );
			if (dwEvent != WAIT_OBJECT_0) {
				::ReleaseSemaphore( m_semReaders, 1, NULL );
				return false;
			}
			//assert(dwEvent == WAIT_OBJECT_0);
		}
		// V( semReaders )
		::ReleaseSemaphore( m_semReaders, 1, NULL );
		return true;
	};

	inline bool Unlock_DataRead() {
		DWORD dwEvent = WAIT_TIMEOUT;
		// P( semReaders )
		dwEvent = ::WaitForSingleObject( m_semReaders, INFINITE );
		assert(dwEvent == WAIT_OBJECT_0);

		m_nReaders--;

		if (m_nReaders == 0)
		{
			// V( semWriters )
			::ReleaseSemaphore(m_semWriters, 1, NULL);
		}
		// V( semReaders )
		::ReleaseSemaphore( m_semReaders, 1, NULL );
		return true;
	};

	inline bool Lock_DataWrite(DWORD dwMilliseconds = INFINITE){
		DWORD dwEvent = WAIT_TIMEOUT;

		// P( semWriters )
		dwEvent = ::WaitForSingleObject( m_semWriters, dwMilliseconds );
		if (dwEvent != WAIT_OBJECT_0)
			return false;
		//assert(dwEvent == WAIT_OBJECT_0);
		return true;
	}

	inline void Unlock_DataWrite(){
		// V( semWriters )
		::ReleaseSemaphore(m_semWriters, 1, NULL);
	};

};

class ReadLock
{
protected:
	MutexRW*	m_pMutexRW;
	bool		m_bIsLocked;
public:
	ReadLock(MutexRW* pMutexRW, const bool bInitialLock = false, DWORD dwMilliseconds = INFINITE)
		:  m_pMutexRW(pMutexRW), m_bIsLocked(false)
	{
		assert(m_pMutexRW);
		if (bInitialLock){
			m_bIsLocked = m_pMutexRW->Lock_DataRead(dwMilliseconds);
		}
	};

	inline const bool& IsLocked() const{
		return m_bIsLocked;
	};

	inline void Lock(DWORD dwMilliseconds = INFINITE){
		assert(m_bIsLocked == false);
		m_bIsLocked = m_pMutexRW->Lock_DataRead(dwMilliseconds);
	};

	inline void Unlock(){
		assert(m_bIsLocked);
		m_pMutexRW->Unlock_DataRead();
		m_bIsLocked = false;
	};
	virtual ~ReadLock(){
		if (m_bIsLocked){
			m_pMutexRW->Unlock_DataRead();
		}
	};
};

class WriteLock
{
protected:
	MutexRW*	m_pMutexRW;
	bool		m_bIsLocked;
public:
	WriteLock(MutexRW* pMutexRW, const bool bInitialLock = false, DWORD dwMilliseconds = INFINITE) 
		:  m_pMutexRW(pMutexRW), m_bIsLocked(false)
	{
		assert(m_pMutexRW);
		if (bInitialLock){
			m_bIsLocked = m_pMutexRW->Lock_DataWrite(dwMilliseconds);
		}
	};

	inline const bool& IsLocked() const{
		return m_bIsLocked;
	};

	inline void Lock(DWORD dwMilliseconds = INFINITE){
		assert(m_bIsLocked == false);
		m_bIsLocked = m_pMutexRW->Lock_DataWrite(dwMilliseconds);
	};

	inline void Unlock(){
		assert(m_bIsLocked);
		m_pMutexRW->Unlock_DataWrite();
		m_bIsLocked = false;
	};
	virtual ~WriteLock(){
		if (m_bIsLocked){
			m_pMutexRW->Unlock_DataWrite();
		}
	};
};
