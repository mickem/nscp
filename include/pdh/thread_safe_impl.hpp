/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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