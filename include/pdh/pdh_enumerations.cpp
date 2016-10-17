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
#include <error.hpp>

#include <buffer.hpp>

#include <pdh/pdh_interface.hpp>
#include <pdh/pdh_enumerations.hpp>

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
			pdh_error status = factory::get_impl()->PdhEnumObjectItems(NULL, NULL, object.name_w().c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
			if (status.is_more_data()) {
				szCounterBuffer = new TCHAR[dwCounterBufLen + 1];
				szInstanceBuffer = new TCHAR[dwInstanceBufLen + 1];

				status = factory::get_impl()->PdhEnumObjectItems(NULL, NULL, object.name_w().c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
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