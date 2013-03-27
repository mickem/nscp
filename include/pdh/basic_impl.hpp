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

namespace PDH {

	class NativeExternalPDH : public PDH::PDHImpl {
	protected:

		typedef LONG PDH_STATUS;
		//typedef struct _PDH_COUNTER_INFO_A;

		typedef PDH_STATUS (WINAPI *fpPdhLookupPerfNameByIndex)(LPCWSTR,DWORD,LPWSTR,LPDWORD);
		typedef PDH_STATUS (WINAPI *fpPdhLookupPerfIndexByName)(LPCWSTR,LPCWSTR,LPDWORD);
		typedef PDH_STATUS (WINAPI *fpPdhExpandCounterPath)(LPCWSTR,LPWSTR,LPDWORD);
		typedef PDH_STATUS (WINAPI *fpPdhGetCounterInfo)(PDH_HCOUNTER,BOOLEAN,LPDWORD,PDH_COUNTER_INFO*);
		typedef PDH_STATUS (WINAPI *fpPdhAddCounter)(PDH::PDH_HQUERY,LPCWSTR,DWORD_PTR,PDH::PDH_HCOUNTER*);
		typedef PDH_STATUS (WINAPI *fpPdhRemoveCounter)(PDH::PDH_HCOUNTER);
		typedef PDH_STATUS (WINAPI *fpPdhGetFormattedCounterValue)(PDH_HCOUNTER,DWORD,LPDWORD,PPDH_FMT_COUNTERVALUE);
		typedef PDH_STATUS (WINAPI *fpPdhOpenQuery)(LPCTSTR,DWORD_PTR,PDH_HQUERY*);
		typedef PDH_STATUS (WINAPI *fpPdhCloseQuery)(PDH_HQUERY);
		typedef PDH_STATUS (WINAPI *fpPdhCollectQueryData)(PDH_HQUERY);
		typedef PDH_STATUS (WINAPI *fpPdhValidatePath)(LPCWSTR);
		typedef PDH_STATUS (WINAPI *fpPdhEnumObjects)(LPCWSTR,LPCWSTR,LPWSTR,LPDWORD,DWORD,BOOL);
		typedef PDH_STATUS (WINAPI *fpPdhEnumObjectItems)(LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,LPDWORD,LPWSTR,LPDWORD,DWORD,DWORD);



		static fpPdhLookupPerfNameByIndex pPdhLookupPerfNameByIndex;
		static fpPdhLookupPerfIndexByName pPdhLookupPerfIndexByName;
		static fpPdhExpandCounterPath pPdhExpandCounterPath;
		static fpPdhGetCounterInfo pPdhGetCounterInfo;
		static fpPdhAddCounter pPdhAddCounter;
		static fpPdhRemoveCounter pPdhRemoveCounter;
		static fpPdhGetFormattedCounterValue pPdhGetFormattedCounterValue;
		static fpPdhOpenQuery pPdhOpenQuery;
		static fpPdhCloseQuery pPdhCloseQuery;
		static fpPdhCollectQueryData pPdhCollectQueryData;
		static fpPdhValidatePath pPdhValidatePath;
		static fpPdhEnumObjects pPdhEnumObjects;
		static fpPdhEnumObjectItems pPdhEnumObjectItems;
		static HMODULE PDH_;

	public:
		NativeExternalPDH() {
			load_procs();
		}

		bool reload() {
			unload_procs();
			load_procs();
			return true;
		}
	protected:

		void unload_procs() {

			pPdhLookupPerfNameByIndex = NULL;
			pPdhLookupPerfIndexByName = NULL;
			pPdhExpandCounterPath = NULL;
			pPdhGetCounterInfo = NULL;
			pPdhAddCounter = NULL;
			pPdhRemoveCounter = NULL;
			pPdhGetFormattedCounterValue = NULL;
			pPdhOpenQuery = NULL;
			pPdhCloseQuery = NULL;
			pPdhCollectQueryData = NULL;
			pPdhValidatePath = NULL;
			pPdhEnumObjects = NULL;
			pPdhEnumObjectItems = NULL;
			
			FreeLibrary(PDH_);
			PDH_ = NULL;
		}

		void load_procs() {
			PDH_ = ::LoadLibrary(_TEXT("PDH"));

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
			pPdhOpenQuery = (fpPdhOpenQuery)::GetProcAddress(PDH_, "PdhOpenQueryW");
			pPdhValidatePath = (fpPdhValidatePath)::GetProcAddress(PDH_, "PdhValidatePathW");
			pPdhEnumObjects = (fpPdhEnumObjects)::GetProcAddress(PDH_, "PdhEnumObjectsW");
			pPdhEnumObjectItems = (fpPdhEnumObjectItems)::GetProcAddress(PDH_, "PdhEnumObjectItemsW");
#else
			pPdhLookupPerfNameByIndex = (fpPdhLookupPerfNameByIndex)::GetProcAddress(PDH_, "PdhLookupPerfNameByIndexA");
			pPdhLookupPerfIndexByName = (fpPdhLookupPerfIndexByName)::GetProcAddress(PDH_, "PdhLookupPerfIndexByNameA");
			pPdhExpandCounterPath = (fpPdhExpandCounterPath)::GetProcAddress(PDH_, "PdhExpandCounterPathA");
			pPdhGetCounterInfo = (fpPdhGetCounterInfo)::GetProcAddress(PDH_, "PdhGetCounterInfoA");
			pPdhAddCounter = (fpPdhAddCounter)::GetProcAddress(PDH_, "PdhAddCounterA");
			pPdhOpenQuery = (fpPdhOpenQuery)::GetProcAddress(PDH_, "PdhOpenQueryA");
			pPdhValidatePath = (fpPdhValidatePath)::GetProcAddress(PDH_, "PdhValidatePathA");
			pPdhEnumObjects = (fpPdhEnumObjects)::GetProcAddress(PDH_, "PdhEnumObjectsA");
			pPdhEnumObjectItems = (fpPdhEnumObjectItems)::GetProcAddress(PDH_, "PdhEnumObjectItemsA");
#endif
			pPdhRemoveCounter = (fpPdhRemoveCounter)::GetProcAddress(PDH_, "PdhRemoveCounter");
			pPdhGetFormattedCounterValue = (fpPdhGetFormattedCounterValue)::GetProcAddress(PDH_, "PdhGetFormattedCounterValue");
			pPdhCloseQuery = (fpPdhCloseQuery)::GetProcAddress(PDH_, "PdhCloseQuery");
			pPdhCollectQueryData = (fpPdhCollectQueryData)::GetProcAddress(PDH_, "PdhCollectQueryData");
		}


	public:


		virtual PDHError PdhLookupPerfIndexByName(LPCTSTR szMachineName,LPCTSTR szName,DWORD *dwIndex) {
			if (pPdhLookupPerfIndexByName == NULL)
				throw pdh_exception("Failed to initialize PdhLookupPerfIndexByName");
			return PDH::PDHError(pPdhLookupPerfIndexByName(szMachineName,szName,dwIndex));
		}

		virtual PDHError PdhLookupPerfNameByIndex(LPCTSTR szMachineName,DWORD dwNameIndex,LPTSTR szNameBuffer,LPDWORD pcchNameBufferSize) {
			if (pPdhLookupPerfNameByIndex == NULL)
				throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex :(");
			return PDH::PDHError(pPdhLookupPerfNameByIndex(szMachineName,dwNameIndex,szNameBuffer,pcchNameBufferSize));
		}

		virtual PDHError PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) {
			if (pPdhExpandCounterPath == NULL)
				throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex :(");
			return PDH::PDHError(pPdhExpandCounterPath(szWildCardPath,mszExpandedPathList,pcchPathListLength));
		}
		virtual PDHError PdhGetCounterInfo(PDH::PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO *lpBuffer) {
			if (pPdhGetCounterInfo == NULL)
				throw pdh_exception("Failed to initialize PdhGetCounterInfo :(");
			return PDH::PDHError(pPdhGetCounterInfo(hCounter,bRetrieveExplainText,pdwBufferSize,lpBuffer));
		}
		virtual PDHError PdhAddCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) {
			if (pPdhAddCounter == NULL)
				throw pdh_exception("Failed to initialize PdhAddCounter :(");
			return PDH::PDHError(pPdhAddCounter(hQuery,szFullCounterPath,dwUserData,phCounter));
		}
		virtual PDHError PdhRemoveCounter(PDH::PDH_HCOUNTER hCounter) {
			if (pPdhRemoveCounter == NULL)
				throw pdh_exception("Failed to initialize PdhRemoveCounter :(");
			return PDH::PDHError(pPdhRemoveCounter(hCounter));
		}
		virtual PDHError PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue) {
			if (pPdhGetFormattedCounterValue == NULL)
				throw pdh_exception("Failed to initialize PdhGetFormattedCounterValue :(");
			return PDH::PDHError(pPdhGetFormattedCounterValue(hCounter, dwFormat, lpdwType, pValue));
		}
		virtual PDHError PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH::PDH_HQUERY * phQuery) {
			if (pPdhOpenQuery == NULL)
				throw pdh_exception("Failed to initialize PdhOpenQuery :(");
			return PDH::PDHError(pPdhOpenQuery(szDataSource, dwUserData, phQuery));
		}
		virtual PDHError PdhCloseQuery(PDH_HQUERY hQuery) {
			if (pPdhCloseQuery == NULL)
				throw pdh_exception("Failed to initialize PdhCloseQuery :(");
			return PDH::PDHError(pPdhCloseQuery(hQuery));
		}
		virtual PDHError PdhCollectQueryData(PDH_HQUERY hQuery) {
			if (pPdhCollectQueryData == NULL)
				throw pdh_exception("Failed to initialize PdhCollectQueryData :(");
			return PDH::PDHError(pPdhCollectQueryData(hQuery));
		}
		virtual PDHError PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload) {
			if (pPdhValidatePath == NULL)
				throw pdh_exception("Failed to initialize PdhValidatePath :(");
			PDH::PDHError status = PDH::PDHError(pPdhValidatePath(szFullPathBuffer));
			if (status.is_error() && force_reload) {
				reload();
				status = PDH::PDHError(pPdhValidatePath(szFullPathBuffer));
			}
			return status;
		}
		virtual PDHError PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel, BOOL bRefresh) {
			if (pPdhEnumObjects == NULL)
				throw pdh_exception("Failed to initialize PdhEnumObjects :(");
			return PDH::PDHError(pPdhEnumObjects(szDataSource, szMachineName, mszObjectList, pcchBufferSize, dwDetailLevel, bRefresh));
		}
		virtual PDHError PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength, LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags) {
			if (pPdhEnumObjectItems == NULL)
				throw pdh_exception("Failed to initialize PdhEnumObjectItems :(");
			return PDH::PDHError(pPdhEnumObjectItems(szDataSource, szMachineName, szObjectName, mszCounterList, pcchCounterListLength, mszInstanceList, pcchInstanceListLength, dwDetailLevel, dwFlags));
		}
	};
}