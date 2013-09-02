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

namespace format {
	inline std::wstring strip_ctrl_chars(std::wstring str) {
		std::wstring ret; ret.reserve(str.size());
		BOOST_FOREACH(wchar_t c, str)
		{
			if (c==0||c==7||c==10||c==11||c==12||c==13||c==127)
				ret.push_back(L'?');
			else
				ret.push_back(c);
		}
		return ret;
	}

	inline std::string strip_ctrl_chars(std::vector<char> str) {
		std::string ret; ret.reserve(str.size());
		BOOST_FOREACH(char c, str)
		{
			if (c==0||c==7||c==10||c==11||c==12||c==13||c==127)
				ret.push_back('?');
			else
				ret.push_back(c);
		}
		return ret;
	}
	inline std::string strip_ctrl_chars(std::string str) {
		std::string ret; ret.reserve(str.size());
		BOOST_FOREACH(char c, str)
		{
			if (c==0||c==7||c==10||c==11||c==12||c==13||c==127)
				ret.push_back('?');
			else
				ret.push_back(c);
		}
		return ret;
	}

	template<class T>
	static void append_list(T &lst, const T &append, const T sep) {
		if (append.empty())
			return;
		if (!lst.empty())
			lst += sep;
		lst += append;
	}
// 	inline void append_list(std::wstring &lst, const std::wstring &append) {
// 		append_list<std::wstring>(lst, append, _T(", "));
// 	}
	inline void append_list(std::string &lst, const std::string &append) {
		append_list<std::string>(lst, append, ", ");
	}
// 	inline void append_list_ex(std::wstring &lst, std::wstring append, std::wstring sep = _T(", ")) {
// 		if (append.empty())
// 			return;
// 		if (!lst.empty())
// 			lst += sep;
// 		lst += append;
// 	}
// 	inline std::wstring format_buffer(const wchar_t* buf, unsigned int len) {
// 		std::wstringstream ss;
// 		std::wstring chars;
// 		for (unsigned int i=0;i<len;i++) {
// 			ss << std::hex << buf[i];
// 			ss << _T(", ");
// 			if (buf[i] >= ' ' && buf[i] <= 'z')
// 				chars += buf[i];
// 			else
// 				chars += '?';
// 			if (i%32==0) {
// 				ss << chars;
// 				ss << _T("\n");
// 				chars = _T("");
// 			}
// 		}
// 		ss << chars;
// 		return ss.str();
// 	}
	inline std::string format_buffer(const char* buf, std::string::size_type len) {
		std::stringstream ss;
		std::string chars;
		for (std::string::size_type i=0;i<len;i++) {
			if (i%32==0) {
				if (i > 0) {
					ss << chars;
					ss << "\n";
				}
				chars = "";
				ss << std::hex << std::setw(8) << std::setfill('0') << i;
				ss << ": ";
			}
			ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(buf[i]));
			ss << ", ";
			if (buf[i] < 30 || buf[i] == 127)
				chars += '?';
			else
				chars += buf[i];
		}
		ss << chars;
		return ss.str();
	}
	inline std::string format_buffer(const std::string &buf) {
		return format_buffer(buf.c_str(), buf.size());
	}
	inline std::string format_buffer(const std::vector<char> &buf) {
		std::stringstream ss;
		std::string chars;
		for (unsigned int i=0;i<buf.size();i++) {
			if (i%32==0) {
				if (i > 0) {
					ss << chars;
					ss << "\n";
				}
				chars = "";
				ss << std::hex << std::setw(8) << std::setfill('0') << i;
				ss << ": ";
			}
			ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(buf[i]));
			ss << ", ";
			if (buf[i] < 30 || buf[i] == 127)
				chars += '?';
			else
				chars += buf[i];
		}
		ss << chars;
		return ss.str();
	}
	inline std::string format_date(boost::posix_time::ptime date, std::string format = "%Y-%m-%d %H:%M:%S") {
		std::locale locale_local("");

		boost::gregorian::date_facet *date_output = new boost::gregorian::date_facet();
		std::locale locale_adjusted(locale_local, date_output);

		std::stringstream date_ss;
		date_ss.imbue(locale_adjusted);

		date_output->format(format.c_str());
		date_ss << date;

		std::string ss = date_ss.str();
		return ss;
	}
// #ifdef WIN32
// 	inline std::wstring format_date(const SYSTEMTIME &time, std::wstring format = _T("%Y-%m-%d %H:%M:%S")) {
// 		TCHAR buf[51];
// 
// 		struct tm tmTime;
// 		memset(&tmTime, 0, sizeof(tmTime));
// 
// 		tmTime.tm_sec = time.wSecond; // seconds after the minute - [0,59]
// 		tmTime.tm_min = time.wMinute; // minutes after the hour - [0,59]
// 		tmTime.tm_hour = time.wHour;  // hours since midnight - [0,23]
// 		tmTime.tm_mday = time.wDay;  // day of the month - [1,31]
// 		tmTime.tm_mon = time.wMonth-1; // months since January - [0,11]
// 		tmTime.tm_year = time.wYear-1900; // years since 1900
// 		tmTime.tm_wday = time.wDayOfWeek; // days since Sunday - [0,6]
// 
// 		size_t l = wcsftime(buf, 50, format.c_str(), &tmTime);
// 		if (l <= 0 || l >= 50)
// 			return _T("");
// 		buf[l] = 0;
// 		return buf;
// 	}
// #endif
	inline std::string format_date(std::time_t time, std::string format = "%Y-%m-%d %H:%M:%S") {
		return format_date(boost::posix_time::from_time_t(time), format);
	}

	static const long long SECS_BETWEEN_EPOCHS = 11644473600;
	static const long long SECS_TO_100NS = 10000000;
	inline unsigned long long filetime_to_time(unsigned long long filetime) {
		return (filetime - (SECS_BETWEEN_EPOCHS * SECS_TO_100NS)) / SECS_TO_100NS;
	}
	inline std::string format_filetime(unsigned long long filetime, std::string format = "%Y-%m-%d %H:%M:%S") {
		if (filetime == 0)
			return "ZERO";
		return format_date(static_cast<time_t>(filetime_to_time(filetime)), format);
	}

	template<class T>
	inline std::wstring format_non_sci(T i) {
		std::wstringstream ss;
		if (i < 10)
			ss.precision(20);
		ss << std::noshowpoint << std::fixed << i;
		std::wstring s = ss.str();
		std::wstring::size_type pos = s.find_last_not_of('0');
		if (pos == std::wstring::npos)
			return s;
		if (s[pos] != '.')
			pos++;
		return s.substr(0, pos);
	}
	template<class T>
	inline std::wstring decode_hex(T i) {
		std::wstringstream ss;
		ss << std::hex << i;
		return ss.str();
	}
// 	template<class T>
// 	inline T decode_time(std::wstring time, unsigned int factor = 1) {
// 		std::wstring::size_type p = time.find_first_of(_T("sSmMhHdDwW"));
// 		std::wstring::size_type pend = time.find_first_not_of(_T("0123456789"));
// 		T value = boost::lexical_cast<T>(pend==std::wstring::npos?time:time.substr(0,pend).c_str());
// 		if (p == std::wstring::npos)
// 			return value * factor;
// 		else if ( (time[p] == 's') || (time[p] == 'S') )
// 			return value * factor;
// 		else if ( (time[p] == 'm') || (time[p] == 'M') )
// 			return value * 60 * factor;
// 		else if ( (time[p] == 'h') || (time[p] == 'H') )
// 			return value * 60 * 60 * factor;
// 		else if ( (time[p] == 'd') || (time[p] == 'D') )
// 			return value * 24 * 60 * 60 * factor;
// 		else if ( (time[p] == 'w') || (time[p] == 'W') )
// 			return value * 7 * 24 * 60 * 60 * factor;
// 		return value * factor;
// 	}
	template<class T>
	inline T decode_time(std::string time, unsigned int factor = 1) {
		std::string::size_type p = time.find_first_of("sSmMhHdDwW");
		std::string::size_type pend = time.find_first_not_of("0123456789");
		T value = boost::lexical_cast<T>(pend==std::string::npos?time:time.substr(0,pend).c_str());
		if (p == std::string::npos)
			return value * factor;
		else if ( (time[p] == 's') || (time[p] == 'S') )
			return value * factor;
		else if ( (time[p] == 'm') || (time[p] == 'M') )
			return value * 60 * factor;
		else if ( (time[p] == 'h') || (time[p] == 'H') )
			return value * 60 * 60 * factor;
		else if ( (time[p] == 'd') || (time[p] == 'D') )
			return value * 24 * 60 * 60 * factor;
		else if ( (time[p] == 'w') || (time[p] == 'W') )
			return value * 7 * 24 * 60 * 60 * factor;
		return value * factor;
	}

#define WEEK	(7 * 24 * 60 * 60 * 1000)
#define DAY		(24 * 60 * 60 * 1000)
#define HOUR	(60 * 60 * 1000)
#define MIN		(60 * 1000)
#define SEC		(1000)
	inline std::string itos_as_time(unsigned long long time) {
		std::stringstream ss;
		if (time > WEEK) {
			unsigned int w = static_cast<unsigned int>(time/WEEK);
			unsigned int d = static_cast<unsigned int>((time-(w*WEEK))/DAY);
			unsigned int h = static_cast<unsigned int>((time-(w*WEEK)-(d*DAY))/HOUR);
			unsigned int m = static_cast<unsigned int>((time-(w*WEEK)-(d*DAY)-(h*HOUR))/MIN);
			ss << w;
			ss << "w " << d << "d " ;
			ss  << std::setfill('0') << std::setw(2);
			ss << h << ":" << m;
		}
		else if (time > DAY) {
			unsigned int d = static_cast<unsigned int>((time)/DAY);
			unsigned int h = static_cast<unsigned int>((time-(d*DAY))/HOUR);
			unsigned int m = static_cast<unsigned int>((time-(d*DAY)-(h*HOUR))/MIN);
			ss << d;
			ss << "d " ;
			ss << std::setfill('0') << std::setw(2);
			ss << h << ":" << m;
		}
		else if (time > HOUR) {
			unsigned int h = static_cast<unsigned int>((time)/HOUR);
			unsigned int m = static_cast<unsigned int>((time-(h*HOUR))/MIN);
			ss << std::setfill('0') << std::setw(2);
			ss << h << ":" << m;
		} else if (time > MIN) {
			ss << std::setfill('0') << std::setw(2);
			ss << "0:" << static_cast<unsigned int>(time/(60 * 1000));
		} else if (time > SEC)
			ss << boost::lexical_cast<std::string>(static_cast<unsigned int>(time/(1000))) << "s";
		else
			ss << static_cast<unsigned int>(time);
		return ss.str();
	}

	inline long long decode_byte_units(const std::string &s) {
		std::string::size_type p = s.find_first_not_of("0123456789");
		if (p == std::string::npos || p == 0)
			return boost::lexical_cast<long long>(s);
		std::string numbers = s.substr(0, p);
		if (s[p] == 'B')
			return boost::lexical_cast<long long>(numbers);
		else if (s[p] == 'K')
			return boost::lexical_cast<long long>(numbers)*1024;
		else if (s[p] == 'M')
			return boost::lexical_cast<long long>(numbers)*1024*1024;
		else if (s[p] == 'G')
			return boost::lexical_cast<long long>(numbers)*1024*1024*1024;
		else if (s[p] == 'T')
			return boost::lexical_cast<long long>(numbers)*1024*1024*1024*1024;
		else
			return boost::lexical_cast<long long>(numbers);
	}
#define BKMG_RANGE "BKMGTP"
#define BKMG_SIZE 5

	template<class T>
	inline std::string format_byte_units(T i) {
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		while ((cpy > 999)&&(idx<BKMG_SIZE)) {
			cpy/=1024;
			idx++;
		}
		std::stringstream ss;
		ss << std::setprecision(3);
		ss << cpy;
		std::string ret = ss.str();
		ret += postfix[idx];
		return ret;
	}
	template<class T>
	inline T convert_to_byte_units(T i) {
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		while ((cpy > 999)&&(idx<BKMG_SIZE)) {
			cpy/=1024;
			idx++;
		}
		return static_cast<T>(cpy);
	}
	template<class T>
	inline T convert_to_byte_units(T i, std::string unit) {
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		if (unit.length() != 1) {
			i;
		}
		double cpy = static_cast<double>(i);
		while ((cpy > 999)&&(idx<BKMG_SIZE)) {
			if (unit[0] == postfix[idx]) {
				return static_cast<T>(cpy);
			}
			cpy/=1024;
			idx++;
		}
		return static_cast<T>(cpy);
	}
	template<class T>
	inline std::string format_byte_units(T i, std::string unit) {
		std::stringstream ss;
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		if (unit.length() != 1) {
			ss << cpy;
			return ss.str();
		}
		for (int i=0;i<BKMG_SIZE;i++) {
			if (unit[0] == postfix[i]) {
				ss << std::setiosflags(std::ios::fixed) << std::setprecision(3) << cpy;
				std::string s = ss.str();
				std::string::size_type pos = s.find_last_not_of("0");
				if (pos != std::string::npos) {
					s = s.substr(0,pos+1);
				}
				return s;
			}
			cpy/=1024;
		}
		ss << cpy;
		return ss.str();
	}
	inline std::string find_proper_unit_BKMG(unsigned long long i) {
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		while ((cpy > 999)&&(idx<BKMG_SIZE)) {
			cpy/=1024;
			idx++;
		}
		return std::string(1, postfix[idx]);
	}

	typedef std::list<std::wstring> splitList;
	inline splitList splitEx(const std::wstring str, const std::wstring key) {
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

	inline std::string join(std::list<std::string> lst, std::string key) {
		std::string ret;
		BOOST_FOREACH(const std::string &s, lst) {
			if (!ret.empty())
				ret += key;
			ret += s;
		}
		return ret;
	}
	inline std::wstring join(std::list<std::wstring> lst, std::wstring key) {
		std::wstring ret;
		BOOST_FOREACH(const std::wstring &s, lst) {
			if (!ret.empty())
				ret += key;
			ret += s;
		}
		return ret;
	}
}
