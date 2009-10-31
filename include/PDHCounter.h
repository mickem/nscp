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

#include <list>
#include <pdh.h>
#include <pdhmsg.h>
#include <sstream>
#include <error.hpp>

namespace PDH {

	class PDHHelpers {
	public:
		static std::list<std::wstring> build_list(TCHAR *buffer, DWORD bufferSize) {
			std::list<std::wstring> ret;
			DWORD prevPos = 0;
			for (unsigned int i = 0; i<bufferSize-1; i++) {
				if (buffer[i] == 0) {
					std::wstring str = &buffer[prevPos];
					ret.push_back(str);
					prevPos = i+1;
				}
			}
			return ret;
		}
	};

	class PDHException {
	private:
		std::wstring str_;
		std::wstring name_;
		PDH_STATUS pdhStatus_;
	public:
		PDHException(std::wstring name, std::wstring str, PDH_STATUS pdhStatus = 0) : name_(name), str_(str), pdhStatus_(pdhStatus) {}
		PDHException(std::wstring str, PDH_STATUS pdhStatus) : str_(str), pdhStatus_(pdhStatus) {}
		PDHException(std::wstring str) : str_(str), pdhStatus_(0) {}
		std::wstring getError() const {
			std::wstring ret;
			if (!name_.empty())
				ret += name_ + _T(": ");
			ret += str_;
			if (pdhStatus_ != 0) {
				ret += _T(": ") + error::format::from_module(_T("PDH.DLL"), pdhStatus_);
			}
			return ret;
		}
	};

	class PDHResolver {
	public:
		//typedef PDH_FUNCTION (*fpPdhLookupPerfNameByIndex)(IN LPCWSTR szMachineName,IN DWORD dwNameIndex,IN LPWSTR szNameBuffer,IN LPDWORD pcchNameBufferSize);
		typedef PDH_STATUS (WINAPI *fpPdhLookupPerfNameByIndex)(LPCWSTR,DWORD,LPWSTR,LPDWORD);
		typedef PDH_STATUS (WINAPI *fpPdhLookupPerfIndexByName)(LPCWSTR,LPCWSTR,LPDWORD);
		static fpPdhLookupPerfNameByIndex pPdhLookupPerfNameByIndex;
		static fpPdhLookupPerfIndexByName pPdhLookupPerfIndexByName;
		static HMODULE PDH_;
	private:
		static void lookup_function() {
			if (pPdhLookupPerfNameByIndex != NULL)
				return;
			PDH_ = ::LoadLibrary(_TEXT("PDH"));
			
			if (PDH_ == NULL) {
				throw PDHException(_T("LoadLibrary for PDH failed: ")+ error::lookup::last_error());
			}
#ifdef UNICODE
			//*(FARPROC *)&pPdhLookupPerfNameByIndex
			pPdhLookupPerfNameByIndex = (fpPdhLookupPerfNameByIndex)::GetProcAddress(PDH_, "PdhLookupPerfNameByIndexW");
			pPdhLookupPerfIndexByName = (fpPdhLookupPerfIndexByName)::GetProcAddress(PDH_, "PdhLookupPerfIndexByNameW");
#else
			pPdhLookupPerfNameByIndex = (fpPdhLookupPerfNameByIndex)::GetProcAddress(PDH_, "PdhLookupPerfNameByIndexA");
			pPdhLookupPerfIndexByName = (fpPdhLookupPerfIndexByName)::GetProcAddress(PDH_, "PdhLookupPerfIndexByNameA");
#endif
			if (pPdhLookupPerfNameByIndex == NULL || pPdhLookupPerfIndexByName == NULL) {
				throw PDHException(_T("Failed to find function: PdhLookupPerfNameByIndex!")+ error::lookup::last_error());
			}
		}
	public:
		static PDH_STATUS PdhLookupPerfNameByIndex(LPCTSTR szMachineName,DWORD dwNameIndex,LPTSTR szNameBuffer,LPDWORD pcchNameBufferSize) {
			PDHResolver::lookup_function();
			if (pPdhLookupPerfNameByIndex == NULL)
				throw PDHException(_T("Failed to initalize PdhLookupPerfNameByIndex :("));
			return pPdhLookupPerfNameByIndex(szMachineName,dwNameIndex,szNameBuffer,pcchNameBufferSize);
		}



		static PDH_STATUS PdhLookupPerfIndexByName(LPCTSTR szMachineName,LPCTSTR szName,DWORD *dwIndex) {
			PDHResolver::lookup_function();
			if (pPdhLookupPerfIndexByName == NULL)
				throw PDHException(_T("Failed to initalize PdhLookupPerfIndexByName :("));
			return pPdhLookupPerfIndexByName(szMachineName,szName,dwIndex);
		}
#define PDH_INDEX_BUF_LEN 2048
		static std::wstring PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex) {
			TCHAR *buffer = new TCHAR[PDH_INDEX_BUF_LEN+1];
			DWORD bufLen = PDH_INDEX_BUF_LEN;
			PDH_STATUS status = PDHResolver::PdhLookupPerfNameByIndex(szMachineName,dwNameIndex,buffer,&bufLen);
			if (status != ERROR_SUCCESS) {
				delete [] buffer;
				throw PDHException(_T("RESOLVER"), _T("PdhLookupPerfNameByIndex: Could not find index: ") + strEx::itos(dwNameIndex), status);
			}
			std::wstring ret = buffer;
			delete [] buffer;
			return ret;
		}
		static std::list<std::wstring> PdhExpandCounterPath(std::wstring szWildCardPath, DWORD buffSize = PDH_INDEX_BUF_LEN) {
			TCHAR *buffer = new TCHAR[buffSize+1];
			DWORD bufLen = buffSize;
			PDH_STATUS status = ::PdhExpandCounterPath(szWildCardPath.c_str(),buffer,&bufLen);
			if (status != ERROR_SUCCESS) {
				delete [] buffer;
				if (buffSize == PDH_INDEX_BUF_LEN && bufLen > buffSize)
					return PdhExpandCounterPath(szWildCardPath, bufLen+10);
				throw PDHException(_T("RESOLVER"), _T("PdhExpandCounterPath: Could not find index: ") + szWildCardPath, status);
			}
			std::list<std::wstring> ret = PDHHelpers::build_list(buffer, bufLen);
			delete [] buffer;
			return ret;
		}

		static DWORD PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR indexName) {
			DWORD ret;
			PDH_STATUS status = PDHResolver::PdhLookupPerfIndexByName(szMachineName,indexName, &ret);
			if (status != ERROR_SUCCESS) {
				throw PDHException(_T("RESOLVER"), std::wstring(_T("PdhLookupPerfNameByIndex: Could not find index: ")) + indexName, status);
			}
			return ret;
		}
	};

	class PDHCounter;
	class PDHCounterListener {
	public:
		virtual void collect(const PDHCounter &counter) = 0;
		virtual void attach(const PDHCounter *counter) = 0;
		virtual void detach(const PDHCounter *counter) = 0;
		virtual DWORD getFormat() const = 0;
	};

	class PDHCounterInfo {
	public:
		DWORD   dwType;
		DWORD   CVersion;
		DWORD   CStatus;
		LONG    lScale;
		LONG    lDefaultScale;
		DWORD_PTR   dwUserData;
		DWORD_PTR   dwQueryUserData;
		std::wstring  szFullPath;

		std::wstring   szMachineName;
		std::wstring   szObjectName;
		std::wstring   szInstanceName;
		std::wstring   szParentInstance;
		DWORD    dwInstanceIndex;
		std::wstring   szCounterName;

		std::wstring  szExplainText;

		PDHCounterInfo(BYTE *lpBuffer, DWORD dwBufferSize, BOOL explainText) {
			PDH_COUNTER_INFO *info = (PDH_COUNTER_INFO*)lpBuffer;
			dwType = info->dwType;
			CVersion = info->CVersion;
			CStatus = info->CStatus;
			lScale = info->lScale;
			lDefaultScale = info->lDefaultScale;
			dwUserData = info->dwUserData;
			dwQueryUserData = info->dwQueryUserData;
			szFullPath = info->szFullPath;
			if (info->szMachineName)
				szMachineName = info->szMachineName;
			if (info->szObjectName)
				szObjectName = info->szObjectName;
			if (info->szInstanceName)
				szInstanceName = info->szInstanceName;
			if (info->szParentInstance)
				szParentInstance = info->szParentInstance;
			dwInstanceIndex = info->dwInstanceIndex;
			if (info->szCounterName)
				szCounterName = info->szCounterName;
			if (explainText) {
				if (info->szExplainText)
					szExplainText = info->szExplainText;
			}
		}
	};

	class PDHCounter
	{
	private:
		HCOUNTER hCounter_;
		std::wstring name_;
		PDH_FMT_COUNTERVALUE data_;
		PDHCounterListener *listener_;

	public:

		PDHCounter(std::wstring name, PDHCounterListener *listener) : name_(name), listener_(listener), hCounter_(NULL){}
		PDHCounter(std::wstring name) : name_(name), listener_(NULL), hCounter_(NULL){}
		virtual ~PDHCounter(void) {
			if (hCounter_ != NULL)
				remove();
		}

		void setListener(PDHCounterListener *listener) {
			listener_ = listener;
		}

		PDHCounterInfo getCounterInfo(BOOL bExplainText = FALSE) {
			if (hCounter_ == NULL)
				throw PDHException(_T("Counter is null!"));
			PDH_STATUS status;
			BYTE *lpBuffer = new BYTE[1025];
			DWORD bufSize = 1024;
			if ((status = PdhGetCounterInfo(hCounter_, bExplainText, &bufSize, (PDH_COUNTER_INFO*)lpBuffer)) != ERROR_SUCCESS) {
				throw PDHException(name_, _T("getCounterInfo failed (no query)"), status);
			}
			return PDHCounterInfo(lpBuffer, bufSize, TRUE);
		}
		const HCOUNTER getCounter() const {
			return hCounter_;
		}
		const std::wstring getName() const {
			return name_;
		}
		void addToQuery(HQUERY hQuery) {
			PDH_STATUS status;
			if (hQuery == NULL)
				throw PDHException(name_, _T("addToQuery failed (no query)."));
			if (hCounter_ != NULL)
				throw PDHException(name_, _T("addToQuery failed (already opened)."));
			if (listener_)
				listener_->attach(this);
			LPCWSTR name = name_.c_str();
			if ((status = PdhAddCounter(hQuery, name, 0, &hCounter_)) != ERROR_SUCCESS) {
				hCounter_ = NULL;
				throw PDHException(name_, _T("PdhAddCounter failed"), status);
			}
			if (hCounter_ == NULL)
				throw PDHException(_T("Counter is null!"));
		}
		void remove() {
			if (hCounter_ == NULL)
				return;
			PDH_STATUS status;
			if (listener_)
				listener_->detach(this);
			if ((status = PdhRemoveCounter(hCounter_)) != ERROR_SUCCESS)
				throw PDHException(name_, _T("PdhRemoveCounter failed"), status);
			hCounter_ = NULL;
		}
		void collect() {
			if (hCounter_ == NULL)
				return;
			PDH_STATUS status;
			if (!listener_)
				return;
			if ((status = PdhGetFormattedCounterValue(hCounter_, listener_->getFormat(), NULL, &data_)) != ERROR_SUCCESS) {
				throw PDHException(name_, _T("PdhGetFormattedCounterValue failed"), status);
			}
			listener_->collect(*this);
		}
		double getDoubleValue() const {
			return data_.doubleValue;
		}
		__int64 getInt64Value() const {
			return data_.largeValue;
		}
		long getIntValue() const {
			return data_.longValue;
		}
		std::wstring getStringValue() const {
			return data_.WideStringValue;
		}
	};

	class PDHQuery 
	{
	private:
		typedef std::list<PDHCounter*> CounterList;
		CounterList counters_;
		HQUERY hQuery_;
	public:
		PDHQuery() : hQuery_(NULL) {
		}
		virtual ~PDHQuery(void) {
			removeAllCounters();
		}

		PDHCounter* addCounter(std::wstring name, PDHCounterListener *listener) {
			PDHCounter *counter = new PDHCounter(name, listener);
			counters_.push_back(counter);
			return counter;
		}
		static std::wstring lookupIndex(DWORD index) {
			return PDHResolver::PdhLookupPerfNameByIndex(NULL, index);
		}
		static DWORD lookupIndex(std::wstring name) {
			return PDHResolver::PdhLookupPerfIndexByName(NULL, name.c_str());
		}
		PDHCounter* addCounter(std::wstring name) {
			PDHCounter *counter = new PDHCounter(name);
			counters_.push_back(counter);
			return counter;
		}
		void removeAllCounters() {
			if (hQuery_)
				close();
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				delete (*it);
			}
			counters_.clear();
		}

		void open() {
			if (hQuery_ != NULL)
				throw PDHException(_T("query is not null!"));
			PDH_STATUS status;
			if( (status = PdhOpenQuery( NULL, 0, &hQuery_ )) != ERROR_SUCCESS)
				throw PDHException(_T("PdhOpenQuery failed"), status);
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->addToQuery(getQueryHandle());
			}
		}

		void close() {
			if (hQuery_ == NULL)
				throw PDHException(_T("query is null!"));
			PDH_STATUS status;
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->remove();
			}
			if( (status = PdhCloseQuery(hQuery_)) != ERROR_SUCCESS)
				throw PDHException(_T("PdhCloseQuery failed"), status);
			hQuery_ = NULL;
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				delete (*it);
			}
			counters_.clear();
		}

		void gatherData() {
			PDH_STATUS status;
			if ((status = PdhCollectQueryData(hQuery_)) != ERROR_SUCCESS)
				throw PDHException(_T("PdhCollectQueryData failed: "), status);
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->collect();
			}
		}
		void collect() {
			PDH_STATUS status;
			if ((status = PdhCollectQueryData(hQuery_)) != ERROR_SUCCESS)
				throw PDHException(_T("PdhCollectQueryData failed: "), status);
		}

		HQUERY getQueryHandle() const {
			return hQuery_;
		}
	};

	class Enumerations {
	public:

		struct Counter {
			std::wstring name;
		};
		typedef std::list<Counter> Counters;
		struct Instance {
			std::wstring name;
		};
		typedef std::list<Instance> Instances;
		struct Object {
			std::wstring name;
			Instances instances;
			Counters counters;
		};

		typedef std::list<Object> Objects;
		static Objects EnumObjects(DWORD dwDetailLevel = PERF_DETAIL_WIZARD) {
			Objects ret;

			DWORD dwObjectBufLen = 0;
			TCHAR* szObjectBuffer = NULL;
			PDH_STATUS status = PdhEnumObjects(NULL, NULL, szObjectBuffer, &dwObjectBufLen, dwDetailLevel, FALSE);
			if (status != PDH_MORE_DATA)
				throw PDHException(_T("PdhEnumObjects failed when trying to retrieve size of object buffer"), status);

			szObjectBuffer = new TCHAR[dwObjectBufLen+1024];
			status = PdhEnumObjects(NULL, NULL, szObjectBuffer, &dwObjectBufLen, dwDetailLevel, FALSE);
			if (status != ERROR_SUCCESS)
				throw PDHException(_T("PdhEnumObjects failed when trying to retrieve object buffer"), status);

			TCHAR *cp=szObjectBuffer;
			while(*cp != '\0') {
				Object o;
				o.name = cp;
				ret.push_back(o);
				cp += lstrlen(cp)+1;
			}
			delete [] szObjectBuffer;

			for (Objects::iterator it = ret.begin(); it != ret.end(); ++it) {
				DWORD dwCounterBufLen = 0;
				TCHAR* szCounterBuffer = NULL;
				DWORD dwInstanceBufLen = 0;
				TCHAR* szInstanceBuffer = NULL;
				status = PdhEnumObjectItems(NULL, NULL, (*it).name.c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
				if (status == PDH_MORE_DATA) {
					szCounterBuffer = new TCHAR[dwCounterBufLen+1024];
					szInstanceBuffer = new TCHAR[dwInstanceBufLen+1024];
					status = PdhEnumObjectItems(NULL, NULL, (*it).name.c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
					if (status != ERROR_SUCCESS)
						throw PDHException(_T("PdhEnumObjectItems failed when trying to retrieve buffer for ") + (*it).name, status);

					if (dwCounterBufLen > 0) {
						cp=szCounterBuffer;
						while(*cp != '\0') {
							Counter o;
							o.name = cp;
							(*it).counters.push_back(o);
							cp += lstrlen(cp)+1;
						}
					}
					if (dwInstanceBufLen > 0) {
						cp=szInstanceBuffer;
						while(*cp != '\0') {
							Instance o;
							o.name = cp;
							(*it).instances.push_back(o);
							cp += lstrlen(cp)+1;
						}
					}
					delete [] szCounterBuffer;
					delete [] szInstanceBuffer;
					//throw PDHException("PdhEnumObjectItems failed when trying to retrieve size for " + (*it).name, status);
				}
			}
			return ret;
		}
		/*
		static str_lst EnumObjectItems(std::wstring object) {
			str_lst ret;
			DWORD bufLen = 4096;
			DWORD bufLen2 = 0;
			LPTSTR buf = new char[bufLen+1];
			PDH_STATUS status = PdhEnumObjectItems(NULL, NULL, object.c_str(), buf, &bufLen, NULL, &bufLen2, PERF_DETAIL_WIZARD, 0);
			if (status == ERROR_SUCCESS) {
				char *cp=buf;
				while(*cp != '\0') {
					ret.push_back(std::wstring(cp));
					cp += lstrlen(cp)+1;
				}
			}
			return ret;
		}
		*/
		struct pdh_object_details {
			typedef std::list<std::wstring> list;
			list counters;
			list instances;
		};
		static pdh_object_details EnumObjectInstances(std::wstring object, DWORD wanted_counter_len = PDH_INDEX_BUF_LEN, DWORD wanted_instance_len = PDH_INDEX_BUF_LEN) {
			DWORD counter_len = wanted_counter_len;
			DWORD instance_len = wanted_instance_len;
			TCHAR *counter_buffer = new TCHAR[counter_len+1];
			TCHAR *instance_buffer = new TCHAR[instance_len+1];
			PDH_STATUS status = PdhEnumObjectItems(NULL, NULL, object.c_str(), counter_buffer, &counter_len, instance_buffer, &instance_len, PERF_DETAIL_WIZARD, 0);
			if (status != ERROR_SUCCESS) {
				delete [] counter_buffer;
				delete [] instance_buffer;
				if (status == PDH_MORE_DATA && wanted_counter_len == PDH_INDEX_BUF_LEN && wanted_instance_len == PDH_INDEX_BUF_LEN)
					return EnumObjectInstances(object, counter_len+10, instance_len+10);
				throw PDHException(_T("RESOLVER"), _T("EnumObjectInstances: Could not find index: ") + object, status);
			}
			pdh_object_details ret;
			ret.counters = PDHHelpers::build_list(counter_buffer, counter_len);
			ret.instances = PDHHelpers::build_list(instance_buffer, instance_len);
			delete [] counter_buffer;
			delete [] instance_buffer;
			return ret;
		}
		static bool validate(std::wstring counter, std::wstring &error) {
			PDH_STATUS status = PdhValidatePath(counter.c_str());
			switch (status) {
				case ERROR_SUCCESS:
					return true;
				case PDH_CSTATUS_NO_INSTANCE:
					error = _T("The specified instance of the performance object was not found.");
					break;
				case PDH_CSTATUS_NO_COUNTER:
					error = _T("The specified counter was not found in the performance object.");
					break;
				case PDH_CSTATUS_NO_OBJECT:
					error = _T("The specified performance object was not found on the computer.");
					break;
				case PDH_CSTATUS_NO_MACHINE:
					error = _T("The specified computer could not be found or connected to.");
					break;
				case PDH_CSTATUS_BAD_COUNTERNAME:
					error = _T("The counter path string could not be parsed.");
					break;
				case PDH_MEMORY_ALLOCATION_FAILURE:
					error = _T("The function is unable to allocate a required temporary buffer.");
					break;
				default:
					error = _T("Unknown error: ") + error::lookup::last_error(status);
					break;
			}
			return false;
		}
	};


}