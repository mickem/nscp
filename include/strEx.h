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
#include <list>
#include <time.h>
#include <algorithm>

#ifdef __GNUC__
#include <iconv.h>
#include <errno.h>
#endif

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include <nscp_string.hpp>

namespace strEx {
	inline void append_list(std::string &lst, const std::string &append, const std::string sep = ", ") {
		if (append.empty())
			return;
		if (!lst.empty())
			lst += sep;
		lst += append;
	}
#ifdef WIN32
	static const unsigned long long SECS_BETWEEN_EPOCHS = 11644473600ull;
	static const unsigned long long SECS_TO_100NS = 10000000ull;
	inline unsigned long long filetime_to_time(unsigned long long filetime) {
		return (filetime - (SECS_BETWEEN_EPOCHS * SECS_TO_100NS)) / SECS_TO_100NS;
	}
#endif
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

	inline std::string itos_non_sci(double i) {
		std::stringstream ss;
		if (i < 10)
			ss.precision(20);
		ss << std::noshowpoint << std::fixed << i;
		std::string s = ss.str();
		std::string::size_type pos = s.find_last_not_of('0');
		if (pos == std::string::npos)
			return s;
		if (s[pos] != '.')
			pos++;
		return s.substr(0, pos);
	}
	inline std::string ihextos(unsigned int i) {
		std::stringstream ss;
		ss << std::hex << i;
		return ss.str();
	}
	inline double stod(std::string s) {
		return boost::lexical_cast<double>(s.c_str());
	}
	inline long long stoi64(std::string s) {
		return boost::lexical_cast<long long>(s.c_str());
	}
	inline unsigned stoui_as_time(std::string time, unsigned int smallest_unit = 1000) {
		std::string::size_type p = time.find_first_of("sSmMhHdDwW");
		std::string::size_type pend = time.find_first_not_of("0123456789");
		unsigned int value = boost::lexical_cast<unsigned int>(pend == std::string::npos ? time : time.substr(0, pend).c_str());
		if (p == std::string::npos)
			return value * smallest_unit;
		else if ((time[p] == 's') || (time[p] == 'S'))
			return value * 1000;
		else if ((time[p] == 'm') || (time[p] == 'M'))
			return value * 60 * 1000;
		else if ((time[p] == 'h') || (time[p] == 'H'))
			return value * 60 * 60 * 1000;
		else if ((time[p] == 'd') || (time[p] == 'D'))
			return value * 24 * 60 * 60 * 1000;
		else if ((time[p] == 'w') || (time[p] == 'W'))
			return value * 7 * 24 * 60 * 60 * 1000;
		return value * smallest_unit;
	}
	inline unsigned stoui_as_time_sec(std::string time, unsigned int smallest_unit = 1) {
		std::string::size_type p = time.find_first_of("sSmMhHdDwW");
		std::string::size_type pend = time.find_first_not_of("0123456789");
		unsigned int value = 0;
		if (pend != std::string::npos && pend > 0)
			value = boost::lexical_cast<unsigned int>(time.substr(0, pend).c_str());
		if (p == std::string::npos)
			return value * smallest_unit;
		else if ((time[p] == 's') || (time[p] == 'S'))
			return value;
		else if ((time[p] == 'm') || (time[p] == 'M'))
			return value * 60;
		else if ((time[p] == 'h') || (time[p] == 'H'))
			return value * 60 * 60;
		else if ((time[p] == 'd') || (time[p] == 'D'))
			return value * 24 * 60 * 60;
		else if ((time[p] == 'w') || (time[p] == 'W'))
			return value * 7 * 24 * 60 * 60;
		return value * smallest_unit;
	}
	inline long stol_as_time_sec(std::string time, unsigned int smallest_unit = 1) {
		if (time.length() > 1 && time[0] == '-')
			return -(long)stoui_as_time_sec(time.substr(1), smallest_unit);
		return stoui_as_time_sec(time, smallest_unit);
	}

	inline unsigned long long stoi64_as_time(std::string time, unsigned int smallest_unit = 1000) {
		std::string::size_type p = time.find_first_of("sSmMhHdDwW");
		if (p == std::string::npos)
			return boost::lexical_cast<long long>(time) * smallest_unit;
		unsigned long long value = boost::lexical_cast<long long>(time.substr(0, p));
		if ((time[p] == 's') || (time[p] == 'S'))
			return value * 1000;
		else if ((time[p] == 'm') || (time[p] == 'M'))
			return value * 60 * 1000;
		else if ((time[p] == 'h') || (time[p] == 'H'))
			return value * 60 * 60 * 1000;
		else if ((time[p] == 'd') || (time[p] == 'D'))
			return value * 24 * 60 * 60 * 1000;
		else if ((time[p] == 'w') || (time[p] == 'W'))
			return value * 7 * 24 * 60 * 60 * 1000;
		return value * smallest_unit;
	}

#define WEEK	(7 * 24 * 60 * 60 * 1000)
#define DAY		(24 * 60 * 60 * 1000)
#define HOUR	(60 * 60 * 1000)
#define MINUTES		(60 * 1000)
#define SEC		(1000)
	inline std::string itos_as_time(unsigned long long time) {
		if (time > WEEK) {
			long long w = time / WEEK;
			long long d = (time - (w*WEEK)) / DAY;
			long long h = (time - (w*WEEK) - (d*DAY)) / HOUR;
			long long m = (time - (w*WEEK) - (d*DAY) - (h*HOUR)) / MINUTES;
			return s::xtos(w) + "w " + s::xtos(d) + "d " + s::xtos(h) + ":" + s::xtos(m);
		} else if (time > DAY) {
			long long d = time / DAY;
			long long h = (time - (d*DAY)) / HOUR;
			long long m = (time - (d*DAY) - (h*HOUR)) / MINUTES;
			return s::xtos(d) + "d " + s::xtos(h) + ":" + s::xtos(m);
		} else if (time > HOUR) {
			long long h = time / HOUR;
			long long m = (time - (h*HOUR)) / MINUTES;
			return s::xtos(h) + ":" + s::xtos(m);
		} else if (time > MINUTES) {
			return "0:" + s::xtos(time / MINUTES);
		} else if (time > SEC)
			return s::xtos(time / SEC) + "s";
		return s::xtos(time);
	}

	template<class T>
	inline void split(T &ret, const std::string str, const std::string key) {
		typename std::string::size_type pos = 0, lpos = 0;
		while ((pos = str.find(key, pos)) != std::string::npos) {
			ret.push_back(str.substr(lpos, pos - lpos));
			lpos = ++pos;
		}
		if (lpos < str.size())
			ret.push_back(str.substr(lpos));
	}

	inline std::pair<std::string, std::string> split(const std::string str, const std::string key) {
		std::string::size_type pos = str.find(key);
		if (pos == std::string::npos)
			return std::pair<std::string, std::string>(str, std::string());
		return std::pair<std::string, std::string>(str.substr(0, pos), str.substr(pos + key.length()));
	}
	typedef std::pair<std::wstring, std::wstring> token;

#define MK_FORMAT_FTD(min, key, val) \
	if (mtm->tm_year > min) \
	strEx::s::replace(format, key, strEx::s::xtos(val));  \
	else  \
	strEx::s::replace(format, key, "0");
#ifdef WIN32
	inline std::string format_time_delta(struct tm *mtm, std::string format = "%Y years %m months %d days %H hours %M minutes") {
		// "Date: %Y-%m-%d %H:%M:%S"
		MK_FORMAT_FTD(70, "%Y", mtm->tm_year);
		MK_FORMAT_FTD(0, "%m", mtm->tm_mon);
		MK_FORMAT_FTD(0, "%d", mtm->tm_mday - 1);
		MK_FORMAT_FTD(0, "%H", mtm->tm_hour);
		MK_FORMAT_FTD(0, "%M", mtm->tm_min);
		MK_FORMAT_FTD(0, "%S", mtm->tm_sec);
		return format;
	}
	inline std::string format_time_delta(time_t time, std::string format = "%Y years %m months %d days %H hours %M minutes") {
		struct tm nt; // = new struct tm;
#if (_MSC_VER > 1300)  // 1300 == VC++ 7.0
		if (gmtime_s(&nt, &time) != 0)
			return "";
#else
		nt = gmtime(&time);
		if (nt == NULL)
			return "";
#endif
		return format_time_delta(&nt, format);
	}
	inline std::string format_filetime_delta(unsigned long long filetime, std::string format = "%Y-%m-%d %H:%M:%S") {
		if (filetime == 0)
			return "ZERO";
		//filetime -= (SECS_BETWEEN_EPOCHS * SECS_TO_100NS);
		filetime /= SECS_TO_100NS;
		return format_time_delta(static_cast<time_t>(filetime), format);
	}
#endif
	template<typename T>
	inline void parse_command(T &cmd_line, std::list<T> &args) {
		boost::tokenizer<boost::escaped_list_separator<wchar_t>, typename T::const_iterator, T > tok(cmd_line, boost::escaped_list_separator<wchar_t>(L'\\', L' ', L'\"'));
		BOOST_FOREACH(T s, tok)
			args.push_back(s);
	}
	template<typename T>
	inline void parse_command(T cmd_line, T &cmd, std::list<T> &args) {
		boost::tokenizer<boost::escaped_list_separator<wchar_t>, typename T::const_iterator, T > tok(cmd_line, boost::escaped_list_separator<wchar_t>(L'\\', L' ', L'\"'));
		bool first = true;
		BOOST_FOREACH(T s, tok) {
			if (first) {
				cmd = s;
				first = false;
			} else {
				args.push_back(s);
			}
		}
	}
}
