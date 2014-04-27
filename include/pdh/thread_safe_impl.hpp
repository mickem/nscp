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

#include <pdh/core.hpp>
#include <error.hpp>
#include <pdh/basic_impl.hpp>
#include <Mutex.h>

namespace PDH {

	class ThreadedSafePDH : public PDH::NativeExternalPDH {
		MutexHandler mutex_;
		typedef std::list<PDHImplSubscriber*> subscriber_list;
		subscriber_list subscribers_;
	private:

	public:
		ThreadedSafePDH() : NativeExternalPDH() {
		}

		bool reload() {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for reload"));
			return reload_unsafe();
		}

		bool reload_unsafe() {
			for(subscriber_list::const_iterator cit = subscribers_.begin(); cit != subscribers_.end(); ++cit)
				(*cit)->on_unload();
			unload_procs();
			load_procs();
			for(subscriber_list::const_iterator cit = subscribers_.begin(); cit != subscribers_.end(); ++cit)
				(*cit)->on_reload();
			return true;
		}
	public:

		virtual void add_listener(PDHImplSubscriber* sub) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for reload"));
			subscribers_.push_back(sub);
		}
		virtual void remove_listener(PDHImplSubscriber* sub) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for reload"));
			for(subscriber_list::iterator it = subscribers_.begin(); it != subscribers_.end(); ++it) {
				if ( (*it) == sub)
					it = subscribers_.erase(it);
				if (it == subscribers_.end())
					break;
			}
		}

		virtual PDHError PdhLookupPerfIndexByName(LPCTSTR szMachineName,LPCTSTR szName,DWORD *dwIndex) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhLookupPerfIndexByName"));
			if (pPdhLookupPerfIndexByName == NULL)
				throw PDHException(_T("Failed to initalize PdhLookupPerfIndexByName"));
			return PDH::PDHError(pPdhLookupPerfIndexByName(szMachineName,szName,dwIndex));
		}

		virtual PDHError PdhLookupPerfNameByIndex(LPCTSTR szMachineName,DWORD dwNameIndex,LPTSTR szNameBuffer,LPDWORD pcchNameBufferSize) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhLookupPerfNameByIndex"));
			if (pPdhLookupPerfNameByIndex == NULL)
				throw PDHException(_T("Failed to initalize PdhLookupPerfNameByIndex :("));
			return PDH::PDHError(pPdhLookupPerfNameByIndex(szMachineName,dwNameIndex,szNameBuffer,pcchNameBufferSize));
		}

		virtual PDHError PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhExpandCounterPath"));
			if (pPdhExpandCounterPath == NULL)
				throw PDHException(_T("Failed to initalize PdhLookupPerfNameByIndex :("));
			return PDH::PDHError(pPdhExpandCounterPath(szWildCardPath,mszExpandedPathList,pcchPathListLength));
		}
		virtual PDHError PdhGetCounterInfo(PDH::PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO *lpBuffer) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhGetCounterInfo"));
			if (pPdhGetCounterInfo == NULL)
				throw PDHException(_T("Failed to initalize PdhGetCounterInfo :("));
			return PDH::PDHError(pPdhGetCounterInfo(hCounter,bRetrieveExplainText,pdwBufferSize,lpBuffer));
		}
		virtual PDHError PdhAddCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhAddCounter"));
			if (pPdhAddCounter == NULL)
				throw PDHException(_T("Failed to initalize PdhAddCounter :("));
			return PDH::PDHError(pPdhAddCounter(hQuery,szFullCounterPath,dwUserData,phCounter));
		}
		virtual PDHError PdhAddEnglishCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhAddEnglishCounter"));
			if (pPdhAddEnglishCounter == NULL)
				throw PDHException(_T("Failed to initalize PdhAddEnglishCounter :("));
			return PDH::PDHError(pPdhAddEnglishCounter(hQuery,szFullCounterPath,dwUserData,phCounter));
		}
		virtual PDHError PdhRemoveCounter(PDH::PDH_HCOUNTER hCounter) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhRemoveCounter"));
			if (pPdhRemoveCounter == NULL)
				throw PDHException(_T("Failed to initalize PdhRemoveCounter :("));
			return PDH::PDHError(pPdhRemoveCounter(hCounter));
		}
		virtual PDHError PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhGetFormattedCounterValue"));
			if (pPdhGetFormattedCounterValue == NULL)
				throw PDHException(_T("Failed to initalize PdhGetFormattedCounterValue :("));
			return PDH::PDHError(pPdhGetFormattedCounterValue(hCounter, dwFormat, lpdwType, pValue));
		}
		virtual PDHError PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH::PDH_HQUERY * phQuery) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhOpenQuery"));
			if (pPdhOpenQuery == NULL)
				throw PDHException(_T("Failed to initalize PdhOpenQuery :("));
			return PDH::PDHError(pPdhOpenQuery(szDataSource, dwUserData, phQuery));
		}
		virtual PDHError PdhCloseQuery(PDH_HQUERY hQuery) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhCloseQuery"));
			if (pPdhCloseQuery == NULL)
				throw PDHException(_T("Failed to initalize PdhCloseQuery :("));
			return PDH::PDHError(pPdhCloseQuery(hQuery));
		}
		virtual PDHError PdhCollectQueryData(PDH_HQUERY hQuery) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhCollectQueryData"));
			if (pPdhCollectQueryData == NULL)
				throw PDHException(_T("Failed to initalize PdhCollectQueryData :("));
			return PDH::PDHError(pPdhCollectQueryData(hQuery));
		}
		virtual PDHError PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhValidatePath"));
			if (pPdhValidatePath == NULL)
				throw PDHException(_T("Failed to initalize PdhValidatePath :("));
			PDH::PDHError status = PDH::PDHError(pPdhValidatePath(szFullPathBuffer));
			if (status.is_error() && force_reload) {
				reload_unsafe();
				status = PDH::PDHError(pPdhValidatePath(szFullPathBuffer));
			}
			return status;
		}
		virtual PDHError PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel, BOOL bRefresh) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhEnumObjects"));
			if (pPdhEnumObjects == NULL)
				throw PDHException(_T("Failed to initalize PdhEnumObjects :("));
			return PDH::PDHError(pPdhEnumObjects(szDataSource, szMachineName, mszObjectList, pcchBufferSize, dwDetailLevel, bRefresh));
		}
		virtual PDHError PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength, LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags) {
			MutexLock lock(mutex_);
			if (!lock.hasMutex())
				throw PDHException(_T("Failed to get mutex for PdhEnumObjectItems"));
			if (pPdhEnumObjectItems == NULL)
				throw PDHException(_T("Failed to initalize PdhEnumObjectItems :("));
			return PDH::PDHError(pPdhEnumObjectItems(szDataSource, szMachineName, szObjectName, mszCounterList, pcchCounterListLength, mszInstanceList, pcchInstanceListLength, dwDetailLevel, dwFlags));
		}
	};
}