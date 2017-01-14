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

#include <list>
#include <pdh.h>
#include <pdhmsg.h>
#include <sstream>
#include <error/error.hpp>
#include <pdh/pdh_resolver.hpp>

#include <utf8.hpp>
#include <str/xtos.hpp>

namespace PDH {
	std::wstring PDHResolver::PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex) {
		TCHAR *buffer = new TCHAR[PDH_INDEX_BUF_LEN + 1];
		DWORD bufLen = PDH_INDEX_BUF_LEN;

		pdh_error status = factory::get_impl()->PdhLookupPerfNameByIndex(szMachineName, dwNameIndex, buffer, &bufLen);
		if (status.is_error()) {
			delete[] buffer;
			throw pdh_exception("PdhLookupPerfNameByIndex: Could not find index: " + str::xtos(dwNameIndex), status);
		}
		std::wstring ret = buffer;
		delete[] buffer;
		return ret;
	}
	std::list<std::string> PDHResolver::PdhExpandCounterPath(std::string szWildCardPath, DWORD buffSize) {
		TCHAR *buffer = new TCHAR[buffSize + 1];
		DWORD bufLen = buffSize;
		pdh_error status = factory::get_impl()->PdhExpandCounterPath(utf8::cvt<std::wstring>(szWildCardPath).c_str(), buffer, &bufLen);
		if (status.is_error()) {
			delete[] buffer;
			if (buffSize == PDH_INDEX_BUF_LEN && bufLen > buffSize)
				return PdhExpandCounterPath(szWildCardPath, bufLen + 10);
			throw pdh_exception("PdhExpandCounterPath: Could not find index: " + utf8::cvt<std::string>(szWildCardPath), status);
		}
		std::list<std::string> ret = helpers::build_list(buffer, bufLen);
		delete[] buffer;
		return ret;
	}

	DWORD PDHResolver::PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR indexName) {
		DWORD ret;
		pdh_error status = factory::get_impl()->PdhLookupPerfIndexByName(szMachineName, indexName, &ret);
		if (status.is_error()) {
			throw pdh_exception("PdhLookupPerfNameByIndex: Could not find index: " + utf8::cvt<std::string>(indexName), status);
		}
		return ret;
	}

	bool PDHResolver::validate(std::wstring counter, std::wstring &error, bool force_reload) {
		pdh_error status = factory::get_impl()->PdhValidatePath(counter.c_str(), force_reload);
		if (status.is_error())
			error = utf8::cvt<std::wstring>(status.get_message());
		return status.is_ok();
	}
	bool is_speacial_char(char c) {
		return c == '\\' || c == '(' || c == ')';
	}

	bool PDHResolver::expand_index(std::string &counter) {
		std::string::size_type pos = 0;
		do {
			std::string::size_type p1 = counter.find_first_of("0123456789", pos);
			if (p1 == std::wstring::npos)
				return true;
			std::string::size_type p2 = counter.find_first_not_of("0123456789", p1);
			if (p2 == std::string::npos)
				p2 = counter.size();
			if (p2 <= p1)
				return false;
			if (p1 > 0) {
				if (!is_speacial_char(counter[p1 - 1])) {
					pos = p2;
					continue;
				}
			}
			if (p2 < counter.size()) {
				if (!is_speacial_char(counter[p2])) {
					pos = p2;
					continue;
				}
			}
			unsigned int index = str::stox<unsigned int>(counter.substr(p1, p2 - p1));
			std::string sindex = PDHResolver::lookupIndex(index);
			counter.replace(p1, p2 - p1, sindex);
			pos = p1 + sindex.size();
		} while (true);
	}

	std::string PDHResolver::lookupIndex(DWORD index) {
		return utf8::cvt<std::string>(PdhLookupPerfNameByIndex(NULL, index));
	}
	DWORD PDHResolver::lookupIndex(std::string name) {
		return PdhLookupPerfIndexByName(NULL, utf8::cvt<std::wstring>(name).c_str());
	}
}