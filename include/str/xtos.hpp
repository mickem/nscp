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

#include <boost/lexical_cast.hpp>

#include <string>
#include <sstream>

namespace str {

	template<class T>
	inline T stox(std::string s) {
		return boost::lexical_cast<T>(s.c_str());
	}
	template<class T>
	inline T stox(std::string s, T def) {
		try {
			return boost::lexical_cast<T>(s.c_str());
		} catch (...) {
			return def;
		}
	}

	template<typename T>
	inline std::string xtos(T i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}

	inline std::string ihextos(unsigned int i) {
		std::stringstream ss;
		ss << std::hex << i;
		return ss.str();
	}

	template<typename T>
	inline std::string xtos_non_sci(T i) {
		std::stringstream ss;
		if (i < 10)
			ss.precision(20);
		ss << std::noshowpoint << std::fixed << i;
		std::string s = ss.str();
		std::string::size_type pos = s.find('.');
		// 1234456 => 1234456
		if (pos == std::string::npos)
			return s;
		// 12340.0000001234 => 12340.000000
		if ((s.length() - pos) > 6)
			s = s.substr(0, pos + 6);

		std::string::size_type dot_pos = s.find_last_of('.');
		// 12345600 -> 12345600
		if (dot_pos == std::string::npos)
			return s;
		pos = s.find_last_not_of('0');
		// 1234.5600 -> 1234.56
		if (pos > dot_pos)
			return s.substr(0, pos + 1);
		// 123.0000 -> 123
		return s.substr(0, pos);
	}
}