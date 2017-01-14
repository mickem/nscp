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

#include <pdh/pdh_interface.hpp>
#include <error/error.hpp>

namespace PDH {
	class NativeExternalPDH : public PDH::impl_interface {
	protected:

		typedef LONG PDH_STATUS;
		//typedef struct _PDH_COUNTER_INFO_A;

		typedef PDH_STATUS(WINAPI *fpPdhLookupPerfNameByIndex)(LPCWSTR, DWORD, LPWSTR, LPDWORD);
		typedef PDH_STATUS(WINAPI *fpPdhLookupPerfIndexByName)(LPCWSTR, LPCWSTR, LPDWORD);
		typedef PDH_STATUS(WINAPI *fpPdhExpandCounterPath)(LPCWSTR, LPWSTR, LPDWORD);
		typedef PDH_STATUS(WINAPI *fpPdhGetCounterInfo)(PDH_HCOUNTER, BOOLEAN, LPDWORD, PDH_COUNTER_INFO*);
		typedef PDH_STATUS(WINAPI *fpPdhAddCounter)(PDH::PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH::PDH_HCOUNTER*);
		typedef PDH_STATUS(WINAPI *fpPdhAddEnglishCounter)(PDH::PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH::PDH_HCOUNTER*);
		typedef PDH_STATUS(WINAPI *fpPdhRemoveCounter)(PDH::PDH_HCOUNTER);
		typedef PDH_STATUS(WINAPI *fpPdhGetRawCounterValue)(PDH_HCOUNTER, LPDWORD, PPDH_RAW_COUNTER);
		typedef PDH_STATUS(WINAPI *fpPdhGetFormattedCounterValue)(PDH_HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE);
		typedef PDH_STATUS(WINAPI *fpPdhOpenQuery)(LPCTSTR, DWORD_PTR, PDH_HQUERY*);
		typedef PDH_STATUS(WINAPI *fpPdhCloseQuery)(PDH_HQUERY);
		typedef PDH_STATUS(WINAPI *fpPdhCollectQueryData)(PDH_HQUERY);
		typedef PDH_STATUS(WINAPI *fpPdhValidatePath)(LPCWSTR);
		typedef PDH_STATUS(WINAPI *fpPdhEnumObjects)(LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, DWORD, BOOL);
		typedef PDH_STATUS(WINAPI *fpPdhEnumObjectItems)(LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, LPWSTR, LPDWORD, DWORD, DWORD);
		typedef PDH_STATUS(WINAPI *fpPdhExpandWildCardPath)(LPCTSTR, LPCTSTR, LPWSTR, LPDWORD, DWORD);

		static fpPdhLookupPerfNameByIndex pPdhLookupPerfNameByIndex;
		static fpPdhLookupPerfIndexByName pPdhLookupPerfIndexByName;
		static fpPdhExpandCounterPath pPdhExpandCounterPath;
		static fpPdhGetCounterInfo pPdhGetCounterInfo;
		static fpPdhAddCounter pPdhAddCounter;
		static fpPdhAddEnglishCounter pPdhAddEnglishCounter;
		static fpPdhRemoveCounter pPdhRemoveCounter;
		static fpPdhGetRawCounterValue pPdhGetRawCounterValue;
		static fpPdhGetFormattedCounterValue pPdhGetFormattedCounterValue;
		static fpPdhOpenQuery pPdhOpenQuery;
		static fpPdhCloseQuery pPdhCloseQuery;
		static fpPdhCollectQueryData pPdhCollectQueryData;
		static fpPdhValidatePath pPdhValidatePath;
		static fpPdhEnumObjects pPdhEnumObjects;
		static fpPdhEnumObjectItems pPdhEnumObjectItems;
		static fpPdhExpandWildCardPath pPdhExpandWildCardPath;
		static HMODULE PDH_;

	public:
		NativeExternalPDH() {
			load_procs();
		}

		virtual bool reload() {
			unload_procs();
			load_procs();
			return true;
		}

		virtual void add_listener(subscriber*) {}
		virtual void remove_listener(subscriber*) {}

	protected:

		void unload_procs() {
			pPdhLookupPerfNameByIndex = NULL;
			pPdhLookupPerfIndexByName = NULL;
			pPdhExpandCounterPath = NULL;
			pPdhGetCounterInfo = NULL;
			pPdhAddCounter = NULL;
			pPdhAddEnglishCounter = NULL;
			pPdhRemoveCounter = NULL;
			pPdhGetRawCounterValue = NULL;
			pPdhGetFormattedCounterValue = NULL;
			pPdhOpenQuery = NULL;
			pPdhCloseQuery = NULL;
			pPdhCollectQueryData = NULL;
			pPdhValidatePath = NULL;
			pPdhEnumObjects = NULL;
			pPdhEnumObjectItems = NULL;
			pPdhExpandWildCardPath = NULL;

			FreeLibrary(PDH_);
			PDH_ = NULL;
		}

		void load_procs() {
			PDH_ = ::LoadLibrary(L"PDH");

			if (PDH_ == NULL) {
				throw pdh_exception("LoadLibrary for PDH failed: " + error::lookup::last_error());
			}
#ifdef UNICODE
			//*(FARPROC *)&pPdhLookupPerfNameByIndex
			pPdhLookupPerfNameByIndex = (fpPdhLookupPerfNameByIndex)::GetProcAddress(PDH_, "PdhLookupPerfNameByIndexW");
			pPdhLookupPerfIndexByName = (fpPdhLookupPerfIndexByName)::GetProcAddress(PDH_, "PdhLookupPerfIndexByNameW");
			pPdhExpandCounterPath = (fpPdhExpandCounterPath)::GetProcAddress(PDH_, "PdhExpandCounterPathW");
			pPdhGetCounterInfo = (fpPdhGetCounterInfo)::GetProcAddress(PDH_, "PdhGetCounterInfoW");
			pPdhAddCounter = (fpPdhAddCounter)::GetProcAddress(PDH_, "PdhAddCounterW");
			pPdhAddEnglishCounter = (fpPdhAddEnglishCounter)::GetProcAddress(PDH_, "PdhAddEnglishCounterW");
			pPdhOpenQuery = (fpPdhOpenQuery)::GetProcAddress(PDH_, "PdhOpenQueryW");
			pPdhValidatePath = (fpPdhValidatePath)::GetProcAddress(PDH_, "PdhValidatePathW");
			pPdhEnumObjects = (fpPdhEnumObjects)::GetProcAddress(PDH_, "PdhEnumObjectsW");
			pPdhEnumObjectItems = (fpPdhEnumObjectItems)::GetProcAddress(PDH_, "PdhEnumObjectItemsW");
			pPdhExpandWildCardPath = (fpPdhExpandWildCardPath)::GetProcAddress(PDH_, "PdhExpandWildCardPathW");
#else
			pPdhLookupPerfNameByIndex = (fpPdhLookupPerfNameByIndex)::GetProcAddress(PDH_, "PdhLookupPerfNameByIndexA");
			pPdhLookupPerfIndexByName = (fpPdhLookupPerfIndexByName)::GetProcAddress(PDH_, "PdhLookupPerfIndexByNameA");
			pPdhExpandCounterPath = (fpPdhExpandCounterPath)::GetProcAddress(PDH_, "PdhExpandCounterPathA");
			pPdhGetCounterInfo = (fpPdhGetCounterInfo)::GetProcAddress(PDH_, "PdhGetCounterInfoA");
			pPdhAddCounter = (fpPdhAddCounter)::GetProcAddress(PDH_, "PdhAddCounterA");
			pPdhAddEnglishCounter = (fpPdhAddEnglishCounter)::GetProcAddress(PDH_, "PdhAddEnglishCounterA");
			pPdhOpenQuery = (fpPdhOpenQuery)::GetProcAddress(PDH_, "PdhOpenQueryA");
			pPdhValidatePath = (fpPdhValidatePath)::GetProcAddress(PDH_, "PdhValidatePathA");
			pPdhEnumObjects = (fpPdhEnumObjects)::GetProcAddress(PDH_, "PdhEnumObjectsA");
			pPdhEnumObjectItems = (fpPdhEnumObjectItems)::GetProcAddress(PDH_, "PdhEnumObjectItemsA");
			pPdhExpandWildCardPath = (fpPdhExpandWildCardPath)::GetProcAddress(PDH_, "PdhExpandWildCardPathA");
#endif
			pPdhRemoveCounter = (fpPdhRemoveCounter)::GetProcAddress(PDH_, "PdhRemoveCounter");
			pPdhGetRawCounterValue = (fpPdhGetRawCounterValue)::GetProcAddress(PDH_, "PdhGetRawCounterValue");
			pPdhGetFormattedCounterValue = (fpPdhGetFormattedCounterValue)::GetProcAddress(PDH_, "PdhGetFormattedCounterValue");
			pPdhCloseQuery = (fpPdhCloseQuery)::GetProcAddress(PDH_, "PdhCloseQuery");
			pPdhCollectQueryData = (fpPdhCollectQueryData)::GetProcAddress(PDH_, "PdhCollectQueryData");
		}

	public:

		virtual pdh_error PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR szName, DWORD *dwIndex) {
			if (pPdhLookupPerfIndexByName == NULL)
				throw pdh_exception("Failed to initialize PdhLookupPerfIndexByName");
			return PDH::pdh_error(pPdhLookupPerfIndexByName(szMachineName, szName, dwIndex));
		}

		virtual pdh_error PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex, LPTSTR szNameBuffer, LPDWORD pcchNameBufferSize) {
			if (pPdhLookupPerfNameByIndex == NULL)
				throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex");
			return PDH::pdh_error(pPdhLookupPerfNameByIndex(szMachineName, dwNameIndex, szNameBuffer, pcchNameBufferSize));
		}

		virtual pdh_error PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) {
			if (pPdhExpandCounterPath == NULL)
				throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex");
			return PDH::pdh_error(pPdhExpandCounterPath(szWildCardPath, mszExpandedPathList, pcchPathListLength));
		}
		virtual pdh_error PdhGetCounterInfo(PDH::PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO *lpBuffer) {
			if (pPdhGetCounterInfo == NULL)
				throw pdh_exception("Failed to initialize PdhGetCounterInfo");
			return PDH::pdh_error(pPdhGetCounterInfo(hCounter, bRetrieveExplainText, pdwBufferSize, lpBuffer));
		}
		virtual pdh_error PdhAddCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) {
			if (pPdhAddCounter == NULL)
				throw pdh_exception("Failed to initialize PdhAddCounter");
			return PDH::pdh_error(pPdhAddCounter(hQuery, szFullCounterPath, dwUserData, phCounter));
		}
		virtual pdh_error PdhAddEnglishCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) {
			if (pPdhAddEnglishCounter == NULL)
				throw pdh_exception("PdhAddEnglishCounter is only available on Vista and later you need to use localized counters.");
			return pdh_error(pPdhAddEnglishCounter(hQuery, szFullCounterPath, dwUserData, phCounter));
		}
		virtual pdh_error PdhRemoveCounter(PDH::PDH_HCOUNTER hCounter) {
			if (pPdhRemoveCounter == NULL)
				throw pdh_exception("Failed to initialize PdhRemoveCounter");
			return PDH::pdh_error(pPdhRemoveCounter(hCounter));
		}
		virtual pdh_error PdhGetRawCounterValue(PDH::PDH_HCOUNTER hCounter, LPDWORD dwFormat, PPDH_RAW_COUNTER  pValue) {
			if (pPdhGetRawCounterValue == NULL)
				throw pdh_exception("Failed to initialize PdhGetRawCounterValue");
			return PDH::pdh_error(pPdhGetRawCounterValue(hCounter, dwFormat, pValue));
		}
		virtual pdh_error PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue) {
			if (pPdhGetFormattedCounterValue == NULL)
				throw pdh_exception("Failed to initialize PdhGetFormattedCounterValue");
			return PDH::pdh_error(pPdhGetFormattedCounterValue(hCounter, dwFormat, lpdwType, pValue));
		}
		virtual pdh_error PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH::PDH_HQUERY * phQuery) {
			if (pPdhOpenQuery == NULL)
				throw pdh_exception("Failed to initialize PdhOpenQuery");
			return PDH::pdh_error(pPdhOpenQuery(szDataSource, dwUserData, phQuery));
		}
		virtual pdh_error PdhCloseQuery(PDH_HQUERY hQuery) {
			if (pPdhCloseQuery == NULL)
				throw pdh_exception("Failed to initialize PdhCloseQuery");
			return PDH::pdh_error(pPdhCloseQuery(hQuery));
		}
		virtual pdh_error PdhCollectQueryData(PDH_HQUERY hQuery) {
			if (pPdhCollectQueryData == NULL)
				throw pdh_exception("Failed to initialize PdhCollectQueryData");
			return PDH::pdh_error(pPdhCollectQueryData(hQuery));
		}
		virtual pdh_error PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload) {
			if (pPdhValidatePath == NULL)
				throw pdh_exception("Failed to initialize PdhValidatePath");
			PDH::pdh_error status = PDH::pdh_error(pPdhValidatePath(szFullPathBuffer));
			if (status.is_error() && force_reload) {
				reload();
				status = PDH::pdh_error(pPdhValidatePath(szFullPathBuffer));
			}
			return status;
		}
		virtual pdh_error PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel, BOOL bRefresh) {
			if (pPdhEnumObjects == NULL)
				throw pdh_exception("Failed to initialize PdhEnumObjects");
			return PDH::pdh_error(pPdhEnumObjects(szDataSource, szMachineName, mszObjectList, pcchBufferSize, dwDetailLevel, bRefresh));
		}
		virtual pdh_error PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength, LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags) {
			if (pPdhEnumObjectItems == NULL)
				throw pdh_exception("Failed to initialize PdhEnumObjectItems");
			return PDH::pdh_error(pPdhEnumObjectItems(szDataSource, szMachineName, szObjectName, mszCounterList, pcchCounterListLength, mszInstanceList, pcchInstanceListLength, dwDetailLevel, dwFlags));
		}
		virtual pdh_error PdhExpandWildCardPath(LPCTSTR szDataSource, LPCTSTR szWildCardPath, LPWSTR  mszExpandedPathList, LPDWORD pcchPathListLength, DWORD dwFlags) {
			if (pPdhExpandWildCardPath == NULL)
				throw pdh_exception("Failed to initialize PdhExpandWildCardPath");
			return PDH::pdh_error(pPdhExpandWildCardPath(szDataSource, szWildCardPath, mszExpandedPathList, pcchPathListLength, dwFlags));
		}
	};
}