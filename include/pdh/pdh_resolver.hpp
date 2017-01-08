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