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
#include <pdh/counters.hpp>

namespace PDH {

	class PDHResolver {
	public:
#define PDH_INDEX_BUF_LEN 2048
		static std::wstring PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex) {
			TCHAR *buffer = new TCHAR[PDH_INDEX_BUF_LEN+1];
			DWORD bufLen = PDH_INDEX_BUF_LEN;
			
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhLookupPerfNameByIndex(szMachineName,dwNameIndex,buffer,&bufLen);
			if (status.is_error()) {
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
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhExpandCounterPath(szWildCardPath.c_str(),buffer,&bufLen);
			if (status.is_error()) {
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
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhLookupPerfIndexByName(szMachineName,indexName, &ret);
			if (status.is_error()) {
				throw PDHException(_T("RESOLVER"), std::wstring(_T("PdhLookupPerfNameByIndex: Could not find index: ")) + indexName, status);
			}
			return ret;
		}

		static bool validate(std::wstring counter, std::wstring &error, bool force_reload) {
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhValidatePath(counter.c_str(), force_reload);
			if (status.is_error())
				error = status.to_wstring();
			return status.is_ok();
		}

		static bool PDHResolver::expand_index(std::wstring &counter) {
			std::wstring::size_type pos = 0;
			do {
				std::wstring::size_type p1 = counter.find_first_of(_T("0123456789"), pos);
				if (p1 == std::wstring::npos)
					return true;
				std::wstring::size_type p2 = counter.find_first_not_of(_T("0123456789"), p1);
				if (p2 == std::wstring::npos)
					p2 = counter.size();
				if (p2 <= p1)
					return false;
				unsigned int index = strEx::stoi(counter.substr(p1, p2-p1));
				std::wstring sindex = PDH::PDHResolver::lookupIndex(index);
				counter.replace(p1, p2-p1, sindex);
				pos = p1 + sindex.size();
			} while (true);
		}

		static std::wstring lookupIndex(DWORD index) {
			return PDHResolver::PdhLookupPerfNameByIndex(NULL, index);
		}
		static DWORD lookupIndex(std::wstring name) {
			return PDHResolver::PdhLookupPerfIndexByName(NULL, name.c_str());
		}

	};
}
