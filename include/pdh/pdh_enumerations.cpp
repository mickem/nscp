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

#include <buffer.hpp>

#include <pdh/pdh_interface.hpp>
#include <pdh/pdh_enumerations.hpp>

#include <utf8.hpp>

#include <boost/foreach.hpp>

namespace PDH {
	std::list<std::string> Enumerations::expand_wild_card_path(const std::string &query, std::string &error) {
		std::list<std::string> ret;
		std::wstring wquery = utf8::cvt<std::wstring>(query);
		hlp::buffer<TCHAR> buffer(1024);
		DWORD dwBufLen = buffer.size();
		try {
			pdh_error status = factory::get_impl()->PdhExpandWildCardPath(NULL, wquery.c_str(), buffer, &dwBufLen, 0);
			if (status.is_more_data()) {
				buffer.resize(dwBufLen + 10);
				dwBufLen = buffer.size();
				status = factory::get_impl()->PdhExpandWildCardPath(NULL, wquery.c_str(), buffer, &dwBufLen, 0);
			}
			if (status.is_not_found()) {
				error = status.get_message();
				status = factory::get_impl()->PdhExpandWildCardPath(NULL, wquery.c_str(), buffer, &dwBufLen, 0);

				HQUERY hQuery;
				status = factory::get_impl()->PdhOpenQuery(NULL, NULL, &hQuery);
				if (status.is_error()) {
					error = status.get_message();
					return ret;
				}

				// TODO Create query: QUERY
				PDH_HCOUNTER hCounter;
				status = factory::get_impl()->PdhAddEnglishCounter(hQuery, wquery.c_str(), NULL, &hCounter);
				if (status.is_error()) {
					error = status.get_message();
					return ret;
				}

				hlp::buffer<TCHAR, PDH_COUNTER_INFO*> tBuf2(2048);
				DWORD bufSize = tBuf2.size();

				status = factory::get_impl()->PdhGetCounterInfo(hCounter, FALSE, &bufSize, tBuf2.get());
				if (status.is_error()) {
					error = status.get_message();
					return ret;
				}
				std::wstring counterName = tBuf2.get()->szFullPath;
				error = "";
				return expand_wild_card_path(utf8::cvt<std::string>(counterName), error);
			}
			if (status.is_error()) {
				error = status.get_message();
				return ret;
			}
			if (dwBufLen > 0) {
				TCHAR *cp = buffer.get();
				while (*cp != L'\0') {
					std::wstring tmp = cp;
					ret.push_back(utf8::cvt<std::string>(tmp));
					cp += wcslen(cp) + 1;
				}
			}
		} catch (std::exception &e) {
			error = utf8::utf8_from_native(e.what());
		} catch (...) {
			error = "Unknown exception";
		}
		return ret;
	}
	void Enumerations::fetch_object_details(Object &object, bool instances, bool objects, DWORD dwDetailLevel) {
		DWORD dwCounterBufLen = 0;
		TCHAR* szCounterBuffer = NULL;
		DWORD dwInstanceBufLen = 0;
		TCHAR* szInstanceBuffer = NULL;
		try {
			pdh_error status = factory::get_impl()->PdhEnumObjectItems(NULL, NULL, utf8::cvt<std::wstring>(object.name).c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
			if (status.is_more_data()) {
				szCounterBuffer = new TCHAR[dwCounterBufLen + 1];
				szInstanceBuffer = new TCHAR[dwInstanceBufLen + 1];

				status = factory::get_impl()->PdhEnumObjectItems(NULL, NULL, utf8::cvt<std::wstring>(object.name).c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
				if (status.is_error()) {
					delete[] szCounterBuffer;
					delete[] szInstanceBuffer;
					object.error = "Failed to enumerate object: " + object.name;
				}

				if (dwCounterBufLen > 0 && objects) {
					TCHAR *cp = szCounterBuffer;
					while (*cp != '\0') {
						object.counters.push_back(utf8::cvt<std::string>(cp));
						cp += lstrlen(cp) + 1;
					}
				}
				if (dwInstanceBufLen > 0 && instances) {
					TCHAR *cp = szInstanceBuffer;
					while (*cp != '\0') {
						object.instances.push_back(utf8::cvt<std::string>(cp));
						cp += lstrlen(cp) + 1;
					}
				}
				delete[] szCounterBuffer;
				delete[] szInstanceBuffer;
			} else {
				object.error = "Failed to enumerate object: " + object.name;
			}
		} catch (std::exception &e) {
			object.error = e.what();
		} catch (...) {
			object.error = "Exception fetching data for: " + object.name;
		}
	}
	Enumerations::Objects Enumerations::EnumObjects(bool instances, bool objects, DWORD dwDetailLevel) {
		Objects ret;

		DWORD dwObjectBufLen = 0;
		TCHAR* szObjectBuffer = NULL;
		pdh_error status = factory::get_impl()->PdhEnumObjects(NULL, NULL, szObjectBuffer, &dwObjectBufLen, dwDetailLevel, FALSE);
		if (!status.is_more_data())
			throw pdh_exception("PdhEnumObjects failed when trying to retrieve size of object buffer", status);

		szObjectBuffer = new TCHAR[dwObjectBufLen + 1024];
		status = factory::get_impl()->PdhEnumObjects(NULL, NULL, szObjectBuffer, &dwObjectBufLen, dwDetailLevel, FALSE);
		if (status.is_error())
			throw pdh_exception("PdhEnumObjects failed when trying to retrieve object buffer", status);

		TCHAR *cp = szObjectBuffer;
		while (*cp != '\0') {
			Object o;
			o.name = utf8::cvt<std::string>(cp);
			ret.push_back(o);
			cp += lstrlen(cp) + 1;
		}
		delete[] szObjectBuffer;

		if (objects || instances) {
			BOOST_FOREACH(Object &o, ret) {
				fetch_object_details(o, instances, objects, dwDetailLevel);
			}
		}
		return ret;
	}

	Enumerations::Object Enumerations::EnumObject(std::string object, bool instances, bool objects, DWORD dwDetailLevel) {
		Object ret;
		ret.name = object;
		fetch_object_details(ret, instances, objects, dwDetailLevel);
		return ret;
	}
}