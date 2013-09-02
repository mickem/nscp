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
#include <unicode_char.hpp>
#include <types.hpp>

#include <string>
#include <sstream>
#include <iomanip>
#include <utility>
#include <list>
#include <functional>
#include <time.h>
#include <algorithm>
#include <locale>
#include <iostream>

#include <format.hpp>
#include <utf8.hpp>

#include <cctype>

#ifdef __GNUC__
#include <iconv.h>
#include <errno.h>
#endif

#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#ifdef _DEBUG
#include <iostream>
#endif

#include <string>
#include <locale>

namespace strEx {
	class string_exception : public std::exception {
		std::wstring _what;
	public:
		string_exception(std::wstring what) : _what(what) {}
		std::wstring what() {
			return _what;
		}
		virtual ~string_exception() throw();
	};
	namespace s {
		template<class T>
		inline std::list<T> splitEx(const T str, const T key) {
			std::list<T> ret;
			typename T::size_type pos = 0, lpos = 0;
			while ((pos = str.find(key, pos)) !=  T::npos) {
				ret.push_back(str.substr(lpos, pos-lpos));
				lpos = ++pos;
			}
			if (lpos < str.size())
				ret.push_back(str.substr(lpos));
			return ret;
		}
		template<class T>
		std::string joinEx(const T &lst, const std::string key) {
			std::string ret;
			BOOST_FOREACH(const std::string &s, lst) {
				if (!ret.empty())
					ret += key;
				ret += s;
			}
			return ret;
		}

		typedef std::pair<std::string,std::string> token;
		inline token getToken(std::string buffer, char split) {
			std::string::size_type pos = std::string::npos;
			pos = buffer.find(split);
			if (pos == std::string::npos)
				return token(buffer, "");
			if (pos == buffer.length()-1)
				return token(buffer.substr(0, pos), "");
			return token(buffer.substr(0, pos), buffer.substr(pos+1));
		}

		template<class T>
		inline T stox(std::string s) {
			return boost::lexical_cast<T>(s.c_str());
		}

		template<typename T>
		inline std::string xtos(T i) {
			std::stringstream ss;
			ss << i;
			return ss.str();
		}
		inline std::string itos_non_sci(double i) {
			std::stringstream ss;
			if (i < 10)
				ss.precision(20);
			ss << std::noshowpoint << std::fixed << i;
			std::string s = ss.str();
			std::string::size_type pos = s.find('.');
			if (pos != std::string::npos && (s.length()-pos) > 6) {
				s = s.substr(0, pos+6);
			}

			pos = s.find_last_not_of('0');
			if (pos == std::wstring::npos)
				return s;
			if (s[pos] != '.')
				pos++;
			return s.substr(0, pos);
		}

		template<class T>
		inline void parse_command(const std::string &cmd_line, T &args) {
			boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string > tok(cmd_line, boost::escaped_list_separator<char>('\\', ' ', '\"'));
			BOOST_FOREACH(std::string s, tok)
				args.push_back(s);
		}
		inline std::list<std::string> parse_command(const std::string &cmd_line) {
			std::list<std::string> args;
			boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string > tok(cmd_line, boost::escaped_list_separator<char>('\\', ' ', '\"'));
			BOOST_FOREACH(std::string s, tok)
				args.push_back(s);
			return args;
		}
	}

	inline void append_list(std::string &lst, const std::string &append, const std::string sep = ", ") {
		if (append.empty())
			return;
		if (!lst.empty())
			lst += sep;
		lst += append;
	}

	static const long long SECS_BETWEEN_EPOCHS = 11644473600;
	static const long long SECS_TO_100NS = 10000000;
	inline unsigned long long filetime_to_time(unsigned long long filetime) {
		return (filetime - (SECS_BETWEEN_EPOCHS * SECS_TO_100NS)) / SECS_TO_100NS;
	}

	inline void replace(std::wstring &string, const std::wstring replace, const std::wstring with) {
		std::wstring::size_type pos = string.find(replace);
		std::wstring::size_type len = replace.length();
		while (pos != std::wstring::npos) {
			string = string.substr(0,pos)+with+string.substr(pos+len);
			if (with.find(replace) != std::wstring::npos) // If the replace containes the key look after the replace!
				pos = string.find(replace, pos+with.length());
			else
				pos = string.find(replace, pos+1);
		}
	}
	inline void replace(std::string &string, const std::string replace, const std::string with) {
		std::string::size_type pos = string.find(replace);
		std::string::size_type len = replace.length();
		while (pos != std::string::npos) {
			string = string.substr(0,pos)+with+string.substr(pos+len);
			if (with.find(replace) != std::string::npos) // If the replace containes the key look after the replace!
				pos = string.find(replace, pos+with.length());
			else
				pos = string.find(replace, pos+1);
		}
	}
	inline std::wstring ctos(wchar_t c) {
		return std::wstring(1, c);
	}
	inline wchar_t stoc(std::wstring str) {
		if (str.length() == 0)
			return L' ';
		return str[0];
	}
	template<class T>
	inline std::wstring itos(T i) {
		std::wstringstream ss;
		ss << i;
		return ss.str();
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
	inline int stoi(std::wstring s) {
		return boost::lexical_cast<int>(s.c_str());
	}
	inline double stod(std::string s) {
		return boost::lexical_cast<double>(s.c_str());
	}
	inline double stod(std::wstring s) {
		return boost::lexical_cast<double>(s.c_str());
	}
	inline long long stoi64(std::string s) {
		return boost::lexical_cast<long long>(s.c_str());
	}
	inline unsigned stoui_as_time(std::string time, unsigned int smallest_unit = 1000) {
		std::wstring::size_type p = time.find_first_of("sSmMhHdDwW");
		std::wstring::size_type pend = time.find_first_not_of("0123456789");
		unsigned int value = boost::lexical_cast<unsigned int>(pend==std::string::npos?time:time.substr(0,pend).c_str());
		if (p == std::wstring::npos)
			return value * smallest_unit;
		else if ( (time[p] == 's') || (time[p] == 'S') )
			return value * 1000;
		else if ( (time[p] == 'm') || (time[p] == 'M') )
			return value * 60 * 1000;
		else if ( (time[p] == 'h') || (time[p] == 'H') )
			return value * 60 * 60 * 1000;
		else if ( (time[p] == 'd') || (time[p] == 'D') )
			return value * 24 * 60 * 60 * 1000;
		else if ( (time[p] == 'w') || (time[p] == 'W') )
			return value * 7 * 24 * 60 * 60 * 1000;
		return value * smallest_unit;
	}
	inline unsigned stoui_as_time_sec(std::string time, unsigned int smallest_unit = 1) {
		std::string::size_type p = time.find_first_of("sSmMhHdDwW");
		std::string::size_type pend = time.find_first_not_of("0123456789");
		unsigned int value = boost::lexical_cast<unsigned int>(pend==std::string::npos?time:time.substr(0,pend).c_str());
		if (p == std::string::npos)
			return value * smallest_unit;
		else if ( (time[p
		] == 's') || (time[p] == 'S') )
		return value;
		else if ( (time[p] == 'm') || (time[p] == 'M') )
			return value * 60;
		else if ( (time[p] == 'h') || (time[p] == 'H') )
			return value * 60 * 60;
		else if ( (time[p] == 'd') || (time[p] == 'D') )
			return value * 24 * 60 * 60;
		else if ( (time[p] == 'w') || (time[p] == 'W') )
			return value * 7 * 24 * 60 * 60;
		return value * smallest_unit;
	}
// 	inline long stol_as_time_sec(std::wstring time, unsigned int smallest_unit = 1) {
// 		if (time.length() > 1 && time[0] == L'-')
// 			return -(long)stoui_as_time_sec(time.substr(1), smallest_unit);
// 		return stoui_as_time_sec(time, smallest_unit);
// 	}
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
		if ( (time[p] == 's') || (time[p] == 'S') )
			return value * 1000;
		else if ( (time[p] == 'm') || (time[p] == 'M') )
			return value * 60 * 1000;
		else if ( (time[p] == 'h') || (time[p] == 'H') )
			return value * 60 * 60 * 1000;
		else if ( (time[p] == 'd') || (time[p] == 'D') )
			return value * 24 * 60 * 60 * 1000;
		else if ( (time[p] == 'w') || (time[p] == 'W') )
			return value * 7 * 24 * 60 * 60 * 1000;
		return value * smallest_unit;
	}

#define WEEK	(7 * 24 * 60 * 60 * 1000)
#define DAY		(24 * 60 * 60 * 1000)
#define HOUR	(60 * 60 * 1000)
#define MIN		(60 * 1000)
#define SEC		(1000)
	inline std::string itos_as_time(unsigned long long time) {
		if (time > WEEK) {
			long long w = time/WEEK;
			long long d = (time-(w*WEEK))/DAY;
			long long h = (time-(w*WEEK)-(d*DAY))/HOUR;
			long long m = (time-(w*WEEK)-(d*DAY)-(h*HOUR))/MIN;
			return s::xtos(w) + "w " + s::xtos(d) + "d " + s::xtos(h) + ":" + s::xtos(m);
		}
		else if (time > DAY) {
			long long d = time/DAY;
			long long h = (time-(d*DAY))/HOUR;
			long long m = (time-(d*DAY)-(h*HOUR))/MIN;
			return s::xtos(d) + "d " + s::xtos(h) + ":" + s::xtos(m);
		}
		else if (time > HOUR) {
			long long h = time/HOUR;
			long long m = (time-(h*HOUR))/MIN;
			return s::xtos(h) + ":" + s::xtos(m);
		} else if (time > MIN) {
			return "0:" + s::xtos(time/MIN);
		} else if (time > SEC)
			return s::xtos(time/SEC) + "s";
		return s::xtos(time);
	}

	typedef std::list<std::wstring> splitList;
	inline std::list<std::wstring> splitEx(const std::wstring str, const std::wstring key) {
		splitList ret;
		std::wstring::size_type pos = 0, lpos = 0;
		while ((pos = str.find(key, pos)) !=  std::wstring::npos) {
			ret.push_back(str.substr(lpos, pos-lpos));
			lpos = ++pos;
		}
		if (lpos < str.size())
			ret.push_back(str.substr(lpos));
		return ret;
	}
	typedef std::vector<std::wstring> splitVector;
	template<class T>
	inline std::vector<T> splitV(const T str, const T key) {
		std::vector<T> ret;
		typename T::size_type pos = 0, lpos = 0;
		while ((pos = str.find(key, pos)) !=  T::npos) {
			ret.push_back(str.substr(lpos, pos-lpos));
			lpos = ++pos;
		}
		if (lpos < str.size())
			ret.push_back(str.substr(lpos));
		return ret;
	}
	inline std::wstring joinEx(splitList lst, std::wstring key) {
		std::wstring ret;
		for (splitList::const_iterator it = lst.begin(); it != lst.end(); ++it) {
			if (!ret.empty())
				ret += key;
			ret += *it;
		}
		return ret;
	}

	template<class T>
	inline std::pair<T,T> split(T str, T key) {
		typename T::size_type pos = str.find(key);
		if (pos == T::npos)
			return std::pair<T,T>(str, T());
		return std::pair<T,T>(str.substr(0, pos), str.substr(pos+key.length()));
	}
	typedef std::pair<std::wstring,std::wstring> token;


#define MK_FORMAT_FTD(min, key, val) \
	if (mtm->tm_year > min) \
	strEx::replace(format, key, strEx::s::xtos(val));  \
	else  \
	strEx::replace(format, key, "0");
#ifdef WIN32
	inline std::string format_time_delta(struct tm *mtm, std::string format = "%Y years %m months %d days %H hours %M minutes") {
		// "Date: %Y-%m-%d %H:%M:%S"
		MK_FORMAT_FTD(70, "%Y", mtm->tm_year);
		MK_FORMAT_FTD(0, "%m", mtm->tm_mon);
		MK_FORMAT_FTD(0, "%d", mtm->tm_mday-1);
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
	/*
#ifdef _DEBUG
	inline void test_getToken(std::wstring in1, char in2, std::wstring out1, std::wstring out2) {
		token t = getToken(in1, in2);
		std::wcout << _T("strEx::test_getToken(") << in1 << _T(", ") << in2 << _T(") : ");
		if ((t.first == out1) && (t.second == out2))
			std::wcout << _T("Succeeded") << std::endl;
		else
			std::wcout << _T("Failed [") << out1 << _T("=") << t.first << _T(", ") << out2 << _T("=") << t.second << _T("]") << std::endl;
	}
	inline void run_test_getToken() {
		test_getToken(_T(""), '&', _T(""), _T(""));
		test_getToken(_T("&"), '&', _T(""), _T(""));
		test_getToken(_T("&&"), '&', _T(""), _T("&"));
		test_getToken(_T("foo"), '&', _T("foo"), _T(""));
		test_getToken(_T("foo&"), '&', _T("foo"), _T(""));
		test_getToken(_T("foo&bar"), '&', _T("foo"), _T("bar"));
		test_getToken(_T("foo&bar&test"), '&', _T("foo"), _T("bar&test"));
	}

	inline void test_replace(std::wstring source, std::wstring replace, std::wstring with, std::wstring out) {
		std::wcout << _T("strEx::test_replace(") << source << _T(", ") << replace << _T(", ") << with << _T(") : ");
		std::wstring s = source;
		strEx::replace(s, replace, with);
		if (s == out)
			std::wcout << _T("Succeeded") << std::endl;
		else
			std::wcout << _T("Failed [") << s << _T("=") << out << _T("]") << std::endl;
	}
	inline void run_test_replace() {
		test_replace(_T(""), _T(""), _T(""), _T(""));
		test_replace(_T("foo"), _T(""), _T(""), _T("foo"));
		test_replace(_T("foobar"), _T("foo"), _T(""), _T("bar"));
		test_replace(_T("foobar"), _T("foo"), _T("bar"), _T("barbar"));
	}

#endif
	*/
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
		BOOST_FOREACH(T s, tok)
		{
			if (first) {
				cmd = s;
				first = false;
			} else {
				args.push_back(s);
			}
		}
	}
}

namespace nscp {
	namespace helpers {
		template <typename T> std::string to_string(const T& arg) {
			try {
				return boost::lexical_cast<std::string>(arg) ;
			}
			catch(...) {
				return "";
			}
		}
		template <typename T> std::string to_string(const std::string& arg) {
			return arg;
		}
		template <typename T> std::string to_string(const std::wstring& arg) {
			return utf8::cvt<std::string>(arg);
		}
		template <typename T> std::string to_string(const wchar_t* arg) {
			return utf8::cvt<std::string>(std::wstring(arg));
		}
		template <typename T> std::wstring to_wstring(const T& arg) {
			try {
				return boost::lexical_cast<std::wstring>(arg) ;
			}
			catch(...) {
				return std::wstring();
			}
		}
		template <typename T> std::wstring to_wstring(const std::wstring& arg) {
			return arg;
		}
		template <typename T> std::wstring to_wstring(const std::string& arg) {
			return utf8::cvt<std::wstring>(arg);
		}
	}
}
