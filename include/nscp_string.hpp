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
// #include <iomanip>
// #include <utility>
#include <list>
// #include <functional>
// #include <time.h>
// #include <algorithm>
// #include <locale>
// #include <iostream>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>


namespace strEx {
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
		template<typename T>
		inline std::string xtos_non_sci(T i) {
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
}
