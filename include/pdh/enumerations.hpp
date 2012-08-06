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

#define PDH_INDEX_BUF_LEN 2048

	class Enumerations {
	public:
		struct Object {
			std::wstring name;
			std::string error;
			std::list<std::wstring> instances;
			std::list<std::wstring> counters;
		};

		typedef std::list<Object> Objects;


		static void fetch_object_details(Object &object, bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD) {
			DWORD dwCounterBufLen = 0;
			TCHAR* szCounterBuffer = NULL;
			DWORD dwInstanceBufLen = 0;
			TCHAR* szInstanceBuffer = NULL;
			try {
				PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhEnumObjectItems(NULL, NULL, object.name.c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
				if (status.is_more_data()) {
					szCounterBuffer = new TCHAR[dwCounterBufLen+1];
					szInstanceBuffer = new TCHAR[dwInstanceBufLen+1];

					status = PDH::PDHFactory::get_impl()->PdhEnumObjectItems(NULL, NULL, object.name.c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
					if (status.is_error()) {
						delete [] szCounterBuffer;
						delete [] szInstanceBuffer;
						object.error = "Failed to enumerate object: " + utf8::cvt<std::string>(object.name);
					}

					if (dwCounterBufLen > 0 && objects) {
						TCHAR *cp=szCounterBuffer;
						while(*cp != '\0') {
							object.counters.push_back(cp);
							cp += lstrlen(cp)+1;
						}
					}
					if (dwInstanceBufLen > 0 && instances) {
						TCHAR *cp=szInstanceBuffer;
						while(*cp != '\0') {
							object.instances.push_back(cp);
							cp += lstrlen(cp)+1;
						}
					}
					delete [] szCounterBuffer;
					delete [] szInstanceBuffer;
				} else {
					object.error = "Failed to enumerate object: " + utf8::cvt<std::string>(object.name);
				}
			} catch (std::exception &e) {
				object.error = e.what();
			} catch (...) {
				object.error = "Exception fetching data for: " + utf8::cvt<std::string>(object.name);
			}
		}
		static Objects EnumObjects(bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD) {
			Objects ret;

			DWORD dwObjectBufLen = 0;
			TCHAR* szObjectBuffer = NULL;
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhEnumObjects(NULL, NULL, szObjectBuffer, &dwObjectBufLen, dwDetailLevel, FALSE);
			if (!status.is_more_data())
				throw PDHException(_T("PdhEnumObjects failed when trying to retrieve size of object buffer"), status);

			szObjectBuffer = new TCHAR[dwObjectBufLen+1024];
			status = PDH::PDHFactory::get_impl()->PdhEnumObjects(NULL, NULL, szObjectBuffer, &dwObjectBufLen, dwDetailLevel, FALSE);
			if (status.is_error())
				throw PDHException(_T("PdhEnumObjects failed when trying to retrieve object buffer"), status);

			TCHAR *cp=szObjectBuffer;
			while(*cp != '\0') {
				Object o;
				o.name = cp;
				ret.push_back(o);
				cp += lstrlen(cp)+1;
			}
			delete [] szObjectBuffer;

			if (objects || instances) {
				BOOST_FOREACH(Object &o, ret) {
					fetch_object_details(o, instances, objects, dwDetailLevel);
				}
			}
			return ret;
		}

		static Object EnumObject(std::wstring object, bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD) {
			Object ret;
			ret.name = object;
			fetch_object_details(ret, instances, objects, dwDetailLevel);
			return ret;
		}
	};
}