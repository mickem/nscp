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
#include <assert.h>
#include <sstream>

namespace PDH {
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

	class PDHCounter;
	class PDHCounterListener {
	public:
		virtual void collect(const PDHCounter &counter) = 0;
		virtual void attach(const PDHCounter &counter) = 0;
		virtual void detach(const PDHCounter &counter) = 0;
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
			assert(hCounter_ != NULL);
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
				listener_->attach(*this);
			LPCWSTR name = name_.c_str();
			if ((status = PdhAddCounter(hQuery, name, 0, &hCounter_)) != ERROR_SUCCESS) {
				hCounter_ = NULL;
				throw PDHException(name_, _T("PdhAddCounter failed"), status);
			}
			assert(hCounter_ != NULL);
		}
		void remove() {
			if (hCounter_ == NULL)
				return;
			PDH_STATUS status;
			if (listener_)
				listener_->detach(*this);
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
			if ((status = PdhGetFormattedCounterValue(hCounter_, listener_->getFormat(), NULL, &data_)) != ERROR_SUCCESS)
				throw PDHException(name_, _T("PdhGetFormattedCounterValue failed"), status);
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
			assert(hQuery_ == NULL);
			PDH_STATUS status;
			if( (status = PdhOpenQuery( NULL, 0, &hQuery_ )) != ERROR_SUCCESS)
				throw PDHException(_T("PdhOpenQuery failed"), status);
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->addToQuery(getQueryHandle());
			}
		}

		void close() {
			assert(hQuery_ != NULL);
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
		static str_lst EnumObjectInstances(std::wstring object) {
			str_lst ret;
			DWORD bufLen = 4096;
			DWORD bufLen2 = 0;
			LPTSTR buf = new char[bufLen+1];
			PDH_STATUS status = PdhEnumObjectItems(NULL, NULL, object.c_str(), NULL, &bufLen2, buf, &bufLen, PERF_DETAIL_WIZARD, 0);
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
				case PDH_CSTATUS_NO_MACHINE:
					error = _T("The specified computer could not be found or connected to.");
					break;
				case PDH_CSTATUS_BAD_COUNTERNAME:
					error = _T("The counter path string could not be parsed.");
					break;
				case PDH_MEMORY_ALLOCATION_FAILURE:
					error = _T("The function is unable to allocate a required temporary buffer.");
					break;
			}
			return false;
		}
	};


}