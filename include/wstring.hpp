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
#include <string>

namespace strEx {
	template<class T>
	inline std::wstring xtos(T i) {
		std::wstringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::list<std::wstring> splitEx(const std::wstring str, const std::wstring key) {
		std::list<std::wstring> ret;
		std::wstring::size_type pos = 0, lpos = 0;
		while ((pos = str.find(key, pos)) !=  std::wstring::npos) {
			ret.push_back(str.substr(lpos, pos-lpos));
			lpos = ++pos;
		}
		if (lpos < str.size())
			ret.push_back(str.substr(lpos));
		return ret;
	}
	template<class T>
	inline T stox(std::wstring s) {
		return boost::lexical_cast<T>(s.c_str());
	}

}
