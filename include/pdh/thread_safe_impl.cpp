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

#include <pdh/thread_safe_impl.hpp>

namespace PDH {
	bool ThreadedSafePDH::reload() {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for reload");
		return reload_unsafe();
	}

	bool ThreadedSafePDH::reload_unsafe() {
		for (subscriber_list::const_iterator cit = subscribers_.begin(); cit != subscribers_.end(); ++cit)
			(*cit)->on_unload();
		unload_procs();
		load_procs();
		for (subscriber_list::const_iterator cit = subscribers_.begin(); cit != subscribers_.end(); ++cit)
			(*cit)->on_reload();
		return true;
	}

	void ThreadedSafePDH::add_listener(subscriber* sub) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for reload");
		subscribers_.push_back(sub);
	}
	void ThreadedSafePDH::remove_listener(subscriber* sub) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for reload");
		for (subscriber_list::iterator it = subscribers_.begin(); it != subscribers_.end(); ++it) {
			if ((*it) == sub)
				it = subscribers_.erase(it);
			if (it == subscribers_.end())
				break;
		}
	}

	pdh_error ThreadedSafePDH::PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR szName, DWORD *dwIndex) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhLookupPerfIndexByName");
		if (pPdhLookupPerfIndexByName == NULL)
			throw pdh_exception("Failed to initialize PdhLookupPerfIndexByName");
		return pdh_error(pPdhLookupPerfIndexByName(szMachineName, szName, dwIndex));
	}

	pdh_error ThreadedSafePDH::PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex, LPTSTR szNameBuffer, LPDWORD pcchNameBufferSize) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhLookupPerfNameByIndex");
		if (pPdhLookupPerfNameByIndex == NULL)
			throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex :(");
		return pdh_error(pPdhLookupPerfNameByIndex(szMachineName, dwNameIndex, szNameBuffer, pcchNameBufferSize));
	}

	pdh_error ThreadedSafePDH::PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhExpandCounterPath");
		if (pPdhExpandCounterPath == NULL)
			throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex :(");
		return pdh_error(pPdhExpandCounterPath(szWildCardPath, mszExpandedPathList, pcchPathListLength));
	}
	pdh_error ThreadedSafePDH::PdhGetCounterInfo(PDH::PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO *lpBuffer) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhGetCounterInfo");
		if (pPdhGetCounterInfo == NULL)
			throw pdh_exception("Failed to initialize PdhGetCounterInfo :(");
		return pdh_error(pPdhGetCounterInfo(hCounter, bRetrieveExplainText, pdwBufferSize, lpBuffer));
	}
	pdh_error ThreadedSafePDH::PdhAddCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhAddCounter");
		if (pPdhAddCounter == NULL)
			throw pdh_exception("Failed to initialize PdhAddCounter :(");
		return pdh_error(pPdhAddCounter(hQuery, szFullCounterPath, dwUserData, phCounter));
	}
	pdh_error ThreadedSafePDH::PdhRemoveCounter(PDH::PDH_HCOUNTER hCounter) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhRemoveCounter");
		if (pPdhRemoveCounter == NULL)
			throw pdh_exception("Failed to initialize PdhRemoveCounter :(");
		return pdh_error(pPdhRemoveCounter(hCounter));
	}
	pdh_error ThreadedSafePDH::PdhGetRawCounterValue(PDH::PDH_HCOUNTER hCounter, LPDWORD dwFormat, PPDH_RAW_COUNTER  pValue) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhGetRawCounterValue");
		if (pPdhGetRawCounterValue == NULL)
			throw pdh_exception("Failed to initialize PdhGetRawCounterValue :(");
		return pdh_error(pPdhGetRawCounterValue(hCounter, dwFormat, pValue));
	}
	pdh_error ThreadedSafePDH::PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhGetFormattedCounterValue");
		if (pPdhGetFormattedCounterValue == NULL)
			throw pdh_exception("Failed to initialize PdhGetFormattedCounterValue :(");
		return pdh_error(pPdhGetFormattedCounterValue(hCounter, dwFormat, lpdwType, pValue));
	}
	pdh_error ThreadedSafePDH::PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH::PDH_HQUERY * phQuery) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhOpenQuery");
		if (pPdhOpenQuery == NULL)
			throw pdh_exception("Failed to initialize PdhOpenQuery :(");
		return pdh_error(pPdhOpenQuery(szDataSource, dwUserData, phQuery));
	}
	pdh_error ThreadedSafePDH::PdhCloseQuery(PDH_HQUERY hQuery) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhCloseQuery");
		if (pPdhCloseQuery == NULL)
			throw pdh_exception("Failed to initialize PdhCloseQuery :(");
		return pdh_error(pPdhCloseQuery(hQuery));
	}
	pdh_error ThreadedSafePDH::PdhCollectQueryData(PDH_HQUERY hQuery) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhCollectQueryData");
		if (pPdhCollectQueryData == NULL)
			throw pdh_exception("Failed to initialize PdhCollectQueryData :(");
		return pdh_error(pPdhCollectQueryData(hQuery));
	}
	pdh_error ThreadedSafePDH::PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhValidatePath");
		if (pPdhValidatePath == NULL)
			throw pdh_exception("Failed to initialize PdhValidatePath :(");
		pdh_error status = pdh_error(pPdhValidatePath(szFullPathBuffer));
		if (status.is_error() && force_reload) {
			reload_unsafe();
			status = pdh_error(pPdhValidatePath(szFullPathBuffer));
		}
		return status;
	}
	pdh_error ThreadedSafePDH::PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel, BOOL bRefresh) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhEnumObjects");
		if (pPdhEnumObjects == NULL)
			throw pdh_exception("Failed to initialize PdhEnumObjects :(");
		return pdh_error(pPdhEnumObjects(szDataSource, szMachineName, mszObjectList, pcchBufferSize, dwDetailLevel, bRefresh));
	}
	pdh_error ThreadedSafePDH::PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength, LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags) {
		boost::unique_lock<boost::shared_mutex> lock(mutex_);
		if (!lock.owns_lock())
			throw pdh_exception("Failed to get mutex for PdhEnumObjectItems");
		if (pPdhEnumObjectItems == NULL)
			throw pdh_exception("Failed to initialize PdhEnumObjectItems :(");
		return pdh_error(pPdhEnumObjectItems(szDataSource, szMachineName, szObjectName, mszCounterList, pcchCounterListLength, mszInstanceList, pcchInstanceListLength, dwDetailLevel, dwFlags));
	}
}