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
#include <pdh/pdh_counters.hpp>

namespace PDH {
	class PDHResolver {
	public:
#define PDH_INDEX_BUF_LEN 2048

		static std::wstring PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex);
		static std::list<std::string> PdhExpandCounterPath(std::string szWildCardPath, DWORD buffSize = PDH_INDEX_BUF_LEN);
		static DWORD PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR indexName);
		static bool validate(std::wstring counter, std::wstring &error, bool force_reload);
		//static bool is_speacial_char(char c);

		static bool PDHResolver::expand_index(std::string &counter);

		static std::string lookupIndex(DWORD index);
		static DWORD lookupIndex(std::string name);
	};
}