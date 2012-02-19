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
			std::string error;
			Instances instances;
			Counters counters;
		};

		typedef std::list<Object> Objects;
		static Objects EnumObjects(DWORD dwDetailLevel = PERF_DETAIL_WIZARD) {
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

			BOOST_FOREACH(Object &o, ret) {
				DWORD dwCounterBufLen = 0;
				TCHAR* szCounterBuffer = NULL;
				DWORD dwInstanceBufLen = 0;
				TCHAR* szInstanceBuffer = NULL;
				try {
					status = PDH::PDHFactory::get_impl()->PdhEnumObjectItems(NULL, NULL, o.name.c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
					if (status.is_more_data()) {
						szCounterBuffer = new TCHAR[dwCounterBufLen+1024];
						szInstanceBuffer = new TCHAR[dwInstanceBufLen+1024];

						status = PDH::PDHFactory::get_impl()->PdhEnumObjectItems(NULL, NULL, o.name.c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
						if (status.is_error()) {
							delete [] szCounterBuffer;
							delete [] szInstanceBuffer;
							throw PDHException(_T("PdhEnumObjectItems failed when trying to retrieve buffer for ") + o.name, status);
						}

						if (dwCounterBufLen > 0) {
							cp=szCounterBuffer;
							while(*cp != '\0') {
								Counter c;
								c.name = cp;
								o.counters.push_back(c);
								cp += lstrlen(cp)+1;
							}
						}
						if (dwInstanceBufLen > 0) {
							cp=szInstanceBuffer;
							while(*cp != '\0') {
								Instance i;
								i.name = cp;
								o.instances.push_back(i);
								cp += lstrlen(cp)+1;
							}
						}
						delete [] szCounterBuffer;
						delete [] szInstanceBuffer;
					}
				} catch (std::exception &e) {
					o.error = e.what();
				} catch (...) {
					o.error = "Exception fetching data";
				}
			}
			return ret;
		}

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
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhEnumObjectItems(NULL, NULL, object.c_str(), counter_buffer, &counter_len, instance_buffer, &instance_len, PERF_DETAIL_WIZARD, 0);
			if (status.is_error()) {
				delete [] counter_buffer;
				delete [] instance_buffer;
				if (status.is_more_data() && wanted_counter_len == PDH_INDEX_BUF_LEN && wanted_instance_len == PDH_INDEX_BUF_LEN)
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
	};
}