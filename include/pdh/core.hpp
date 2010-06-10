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

	typedef HANDLE PDH_HCOUNTER;
	typedef HANDLE PDH_HQUERY;
	typedef HANDLE PDH_HLOG;

	class PDHError {
		std::wstring message_;
		bool error_;
		bool more_data_;
	public:
		/*
		PDHError(std::wstring message, bool error, bool more_data) : message_(message), error_(error), more_data_(more_data) {}
		PDHError(bool error, bool more_data) : error_(error), more_data_(more_data) {}
		*/
		PDHError() : error_(false) {}
		PDHError(PDH_STATUS status) : error_(status!=ERROR_SUCCESS), more_data_(status==PDH_MORE_DATA) {
			if (is_error()) {
				message_ = error::format::from_module(_T("PDH.DLL"), status);
			}
		}
		PDHError(const PDHError &other) : error_(other.error_), more_data_(other.more_data_), message_(other.message_) {}
		PDHError& operator=(PDHError const& other) {
			error_ = other.error_;
			more_data_ = other.more_data_;
			message_ = other.message_;
			return *this;
		}


		bool is_error() const {
			return error_;
		}
		bool is_ok() const {
			return !error_;
		}
		std::wstring to_wstring() const {
			return message_;
		}
		void set_message(std::wstring message) {
			message_ = message;
		}
		bool is_more_data() {
			return more_data_;
		}
	};
	class PDHException {
	private:
		std::wstring str_;
		std::wstring name_;
		PDH::PDHError error_;
		//PDH_STATUS pdhStatus_;
	public:
		PDHException(std::wstring name, std::wstring str, PDH::PDHError error) : name_(name), str_(str), error_(error) {}
		PDHException(std::wstring name, LPCWSTR str) : name_(name), str_(str) {}
		PDHException(std::wstring name, std::wstring str) : name_(name), str_(str) {}
		PDHException(std::wstring str, PDH::PDHError error) : str_(str), error_(error) {}
		PDHException(std::wstring str) : str_(str) {}
		std::wstring getError() const {
			std::wstring ret;
			if (!name_.empty())
				ret += name_ + _T(": ");
			ret += str_;
			if (error_.is_error()) {
				ret += _T(": ") + error_.to_wstring();
			}
			return ret;
		}
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

	class PDHImplSubscriber {
	public:
		virtual void on_unload() = 0;
		virtual void on_reload() = 0;
	};

	class PDHImpl {
	public:
		virtual PDHError PdhLookupPerfIndexByName(LPCTSTR szMachineName,LPCTSTR szName,DWORD *dwIndex) = 0;
		virtual PDHError PdhLookupPerfNameByIndex(LPCTSTR szMachineName,DWORD dwNameIndex,LPTSTR szNameBuffer,LPDWORD pcchNameBufferSize) = 0;
		virtual PDHError PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) = 0;
		virtual PDHError PdhGetCounterInfo(PDH::PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO* lpBuffer) = 0;
		virtual PDHError PdhAddCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) = 0;
		virtual PDHError PdhRemoveCounter(PDH::PDH_HCOUNTER hCounter) = 0;
		virtual PDHError PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue) = 0;
		virtual PDHError PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH::PDH_HQUERY * phQuery) = 0;
		virtual PDHError PdhCloseQuery(PDH_HQUERY hQuery) = 0;
		virtual PDHError PdhCollectQueryData(PDH_HQUERY hQuery) = 0;
		virtual PDHError PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload) = 0;
		virtual PDHError PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel, BOOL bRefresh) = 0;
		virtual PDHError PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength, LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags) = 0;

		virtual bool reload() = 0;
		virtual void add_listener(PDHImplSubscriber* sub) = 0;
		virtual void remove_listener(PDHImplSubscriber* sub) = 0;
	};

	class PDHFactory {
		static PDH::PDHImpl *instance;
	public:
		static PDH::PDHImpl *get_impl();
		static void set_threadSafe();
	};

	class PDHHelpers {
	public:
		static std::list<std::wstring> build_list(TCHAR *buffer, DWORD bufferSize) {
			std::list<std::wstring> ret;
			if (bufferSize == 0)
				return ret;
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
}