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