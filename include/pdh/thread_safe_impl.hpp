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

#include <boost/thread.hpp>

#include <pdh/pdh_interface.hpp>
#include <error/error.hpp>
#include <pdh/basic_impl.hpp>

namespace PDH {
	class ThreadedSafePDH : public PDH::NativeExternalPDH {
		boost::shared_mutex mutex_;
		typedef std::list<subscriber*> subscriber_list;
		subscriber_list subscribers_;
	private:

	public:
		ThreadedSafePDH() : NativeExternalPDH() {}

		bool reload();
		bool reload_unsafe();
	public:

		virtual void add_listener(subscriber* sub);
		virtual void remove_listener(subscriber* sub);

		virtual pdh_error PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR szName, DWORD *dwIndex);
		virtual pdh_error PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex, LPTSTR szNameBuffer, LPDWORD pcchNameBufferSize);
		virtual pdh_error PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength);
		virtual pdh_error PdhGetCounterInfo(PDH::PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO *lpBuffer);
		virtual pdh_error PdhAddCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter);
		virtual pdh_error PdhRemoveCounter(PDH::PDH_HCOUNTER hCounter);
		virtual pdh_error PdhGetRawCounterValue(PDH::PDH_HCOUNTER hCounter, LPDWORD dwFormat, PPDH_RAW_COUNTER  pValue);
		virtual pdh_error PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue);
		virtual pdh_error PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH::PDH_HQUERY * phQuery);
		virtual pdh_error PdhCloseQuery(PDH_HQUERY hQuery);
		virtual pdh_error PdhCollectQueryData(PDH_HQUERY hQuery);
		virtual pdh_error PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload);
		virtual pdh_error PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel, BOOL bRefresh);
		virtual pdh_error PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength, LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags);
	};
}