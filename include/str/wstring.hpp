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
#include <string>
#include <list>

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
	inline void replace(std::wstring &string, const std::wstring replace, const std::wstring with) {
		std::wstring::size_type pos = string.find(replace);
		std::wstring::size_type len = replace.length();
		while (pos != std::wstring::npos) {
			string = string.substr(0, pos) + with + string.substr(pos + len);
			if (with.find(replace) != std::wstring::npos) // If the replace containes the key look after the replace!
				pos = string.find(replace, pos + with.length());
			else
				pos = string.find(replace, pos + 1);
		}
	}

}
