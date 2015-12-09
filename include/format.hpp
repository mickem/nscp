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
		BOOST_FOREACH(wchar_t c, str) {
			if (c == 0 || c == 7 || c == 10 || c == 11 || c == 12 || c == 13 || c == 127)
				ret.push_back(L'?');
			else
				ret.push_back(c);
		}
		return ret;
	}

	inline std::string strip_ctrl_chars(std::vector<char> str) {
		std::string ret; ret.reserve(str.size());
		BOOST_FOREACH(char c, str) {
			if (c == 0 || c == 7 || c == 10 || c == 11 || c == 12 || c == 13 || c == 127)
				ret.push_back('?');
			else
				ret.push_back(c);
		}
		return ret;
	}
	inline std::string strip_ctrl_chars(std::string str) {
		std::string ret; ret.reserve(str.size());
		BOOST_FOREACH(char c, str) {
			if (c == 0 || c == 7 || c == 10 || c == 11 || c == 12 || c == 13 || c == 127)
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
	inline void append_list(std::string &lst, const std::string &append) {
		append_list<std::string>(lst, append, ", ");
	}
	inline std::string format_buffer(const char* buf, std::string::size_type len) {
		std::stringstream ss;
		std::string chars;
		for (std::string::size_type i = 0; i < len; i++) {
			if (i % 32 == 0) {
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
		for (unsigned int i = 0; i < buf.size(); i++) {
			if (i % 32 == 0) {
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
	inline std::string format_date(std::time_t time, std::string format = "%Y-%m-%d %H:%M:%S") {
		return format_date(boost::posix_time::from_time_t(time), format);
	}

	static const unsigned long long SECS_BETWEEN_EPOCHS = 11644473600ull;
	static const unsigned long long SECS_TO_100NS = 10000000ull;
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
	template<class T>
	inline T decode_time(std::string time, unsigned int factor = 1) {
		std::string::size_type p = time.find_first_of("sSmMhHdDwW");
		std::string::size_type pend = time.find_first_not_of("0123456789");
		T value = boost::lexical_cast<T>(pend == std::string::npos ? time : time.substr(0, pend).c_str());
		if (p == std::string::npos)
			return value * factor;
		else if ((time[p] == 's') || (time[p] == 'S'))
			return value * factor;
		else if ((time[p] == 'm') || (time[p] == 'M'))
			return value * 60 * factor;
		else if ((time[p] == 'h') || (time[p] == 'H'))
			return value * 60 * 60 * factor;
		else if ((time[p] == 'd') || (time[p] == 'D'))
			return value * 24 * 60 * 60 * factor;
		else if ((time[p] == 'w') || (time[p] == 'W'))
			return value * 7 * 24 * 60 * 60 * factor;
		return value * factor;
	}

#define WEEK	(7 * 24 * 60 * 60 * 1000)
#define DAY		(24 * 60 * 60 * 1000)
#define HOUR	(60 * 60 * 1000)
#define MINUTE	(60 * 1000)
#define SEC		(1000)
	inline std::string itos_as_time(unsigned long long time) {
		std::stringstream ss;
		if (time > WEEK) {
			unsigned int w = static_cast<unsigned int>(time / WEEK);
			unsigned int d = static_cast<unsigned int>((time - (w*WEEK)) / DAY);
			unsigned int h = static_cast<unsigned int>((time - (w*WEEK) - (d*DAY)) / HOUR);
			unsigned int m = static_cast<unsigned int>((time - (w*WEEK) - (d*DAY) - (h*HOUR)) / MINUTE);
			ss << w;
			ss << "w " << d << "d ";
			ss << std::setfill('0') << std::setw(2);
			ss << h << ":" << m;
		} else if (time > DAY) {
			unsigned int d = static_cast<unsigned int>((time) / DAY);
			unsigned int h = static_cast<unsigned int>((time - (d*DAY)) / HOUR);
			unsigned int m = static_cast<unsigned int>((time - (d*DAY) - (h*HOUR)) / MINUTE);
			ss << d;
			ss << "d ";
			ss << std::setfill('0') << std::setw(2);
			ss << h << ":" << m;
		} else if (time > HOUR) {
			unsigned int h = static_cast<unsigned int>((time) / HOUR);
			unsigned int m = static_cast<unsigned int>((time - (h*HOUR)) / MINUTE);
			ss << std::setfill('0') << std::setw(2);
			ss << h << ":" << m;
		} else if (time > MINUTE) {
			ss << std::setfill('0') << std::setw(2);
			ss << "0:" << static_cast<unsigned int>(time / (60 * 1000));
		} else if (time > SEC)
			ss << boost::lexical_cast<std::string>(static_cast<unsigned int>(time / (1000))) << "s";
		else
			ss << static_cast<unsigned int>(time);
		return ss.str();
	}
	template<class T>
	inline T decode_byte_units(const T value, const std::string &unit) {
		if (unit.size() == 0)
			return value;
		if (unit[0] == 'B' || unit[0] == 'b')
			return value;
		else if (unit[0] == 'K' || unit[0] == 'k')
			return value * 1024;
		else if (unit[0] == 'M' || unit[0] == 'm')
			return value * 1024 * 1024;
		else if (unit[0] == 'G' || unit[0] == 'g')
			return value * 1024 * 1024 * 1024;
		else if (unit[0] == 'T' || unit[0] == 't')
			return value * 1024 * 1024 * 1024 * 1024;
		else
			return value;
	}
	inline long long decode_byte_units(const std::string &s) {
		std::string::size_type p = s.find_first_not_of("0123456789");
		if (p == std::string::npos || p == 0)
			return boost::lexical_cast<long long>(s);
		std::string numbers = s.substr(0, p);
		return decode_byte_units(boost::lexical_cast<long long>(numbers), s.substr(p));
	}
#define BKMG_RANGE "BKMGTPE"
#define BKMG_SIZE 5

	inline std::string format_byte_units(const long long i) {
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		double acpy = cpy < 0 ? -cpy : cpy;
		while ((acpy > 999) && (idx < BKMG_SIZE)) {
			cpy /= 1024;
			acpy = cpy < 0 ? -cpy : cpy;
			idx++;
		}
		std::stringstream ss;
		ss << std::setiosflags(std::ios::fixed) << std::setprecision(3) << cpy;
		std::string ret = ss.str();
		std::string::size_type pos = ret.find_last_not_of("0");
		if (pos != std::string::npos) {
			if (ret[pos] == '.')
				pos--;
			ret = ret.substr(0, pos + 1);
		}
		ret += postfix[idx];
		if (idx > 0)
			ret += "B";
		return ret;
	}
	inline std::string format_byte_units(const unsigned long long i) {
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		while ((cpy > 999) && (idx < BKMG_SIZE)) {
			cpy /= 1024;
			idx++;
		}
		std::stringstream ss;
		ss << std::setiosflags(std::ios::fixed) << std::setprecision(3) << cpy;
		std::string ret = ss.str();
		std::string::size_type pos = ret.find_last_not_of("0");
		if (pos != std::string::npos) {
			if (ret[pos] == '.')
				pos--;
			ret = ret.substr(0, pos + 1);
		}
		ret += postfix[idx];
		if (idx > 0)
			ret += "B";
		return ret;
	}
	template<class T>
	inline double convert_to_byte_units(T i, const std::string unit) {
		std::string unit_uc = boost::to_upper_copy(unit);
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		if (unit_uc.size() == 0) {
			return i;
		}
		double cpy = static_cast<double>(i);
		while (idx < BKMG_SIZE) {
			if (unit_uc[0] == postfix[idx]) {
				return cpy;
			}
			cpy /= 1024.0;
			idx++;
		}
		return cpy;
	}
	template<class T>
	inline std::string format_byte_units(T i, std::string unit) {
		std::stringstream ss;
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		if (unit.size() == 0) {
			ss << cpy;
			return ss.str();
		}
		for (int i = 0; i < BKMG_SIZE; i++) {
			if (unit[0] == postfix[i]) {
				ss << std::setiosflags(std::ios::fixed) << std::setprecision(3) << cpy;
				std::string s = ss.str();
				std::string::size_type pos = s.find_last_not_of("0");
				if (pos != std::string::npos) {
					s = s.substr(0, pos + 1);
				}
				return s;
			}
			cpy /= 1024;
		}
		ss << cpy;
		return ss.str();
	}
	inline std::string find_proper_unit_BKMG(unsigned long long i) {
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		while ((cpy > 999) && (idx < BKMG_SIZE)) {
			cpy /= 1024;
			idx++;
		}
		std::string ret = std::string(1, postfix[idx]);
		if (idx > 0)
			ret += "B";
		return ret;
	}

	typedef std::list<std::wstring> splitList;
	inline splitList splitEx(const std::wstring str, const std::wstring key) {
		splitList ret;
		std::wstring::size_type pos = 0, lpos = 0;
		while ((pos = str.find(key, pos)) != std::wstring::npos) {
			ret.push_back(str.substr(lpos, pos - lpos));
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
		while ((pos = str.find(key, pos)) != T::npos) {
			ret.push_back(str.substr(lpos, pos - lpos));
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