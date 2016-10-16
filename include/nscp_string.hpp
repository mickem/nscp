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
#include <list>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>


namespace strEx {
	namespace s {
		inline boost::tuple<std::string, std::string> split2(const std::string str, const std::string key) {
			std::string::size_type pos = str.find(key);
			if (pos == std::string::npos) {
				return boost::make_tuple(str, "");
			}
			return boost::make_tuple(str.substr(0, pos), str.substr(pos+1));
		}
		inline std::list<std::string> splitEx(const std::string str, const std::string key) {
			std::list<std::string> ret;
			std::string::size_type pos = 0, lpos = 0;
			while ((pos = str.find(key, pos)) != std::string::npos) {
				ret.push_back(str.substr(lpos, pos-lpos));
				lpos = ++pos;
			}
			if (lpos < str.size())
				ret.push_back(str.substr(lpos));
			return ret;
		}
		template<class T>
		T split(const std::string str, const std::string key) {
			T ret;
			std::string::size_type pos = 0, lpos = 0;
			while ((pos = str.find(key, pos)) != std::string::npos) {
				ret.push_back(str.substr(lpos, pos - lpos));
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
			if ((s.length()-pos) > 6)
				s = s.substr(0, pos+6);

			std::string::size_type dot_pos = s.find_last_of('.');
			// 12345600 -> 12345600
			if (dot_pos == std::string::npos)
				return s;
			pos = s.find_last_not_of('0');
			// 1234.5600 -> 1234.56
			if (pos > dot_pos)
				return s.substr(0, pos+1);
			// 123.0000 -> 123
			return s.substr(0, pos);
		}

		template<class T>
		inline void parse_command(const std::string &cmd_line, T &args) {
			boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string > tok(cmd_line, boost::escaped_list_separator<char>('\\', ' ', '\"'));
			BOOST_FOREACH(std::string s, tok) {
				if (!s.empty())
					args.push_back(s);
			}
		}
		inline std::list<std::string> parse_command(const std::string &cmd_line) {
			std::list<std::string> args;
			boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string > tok(cmd_line, boost::escaped_list_separator<char>('\\', ' ', '\"'));
			BOOST_FOREACH(std::string s, tok) {
				if (!s.empty())
					args.push_back(s);
			}
			return args;
		}

		inline void replace(std::string &string, const std::string replace, const std::string with) {
			std::string::size_type pos = string.find(replace);
			std::string::size_type len = replace.length();
			while (pos != std::string::npos) {
				string = string.substr(0, pos) + with + string.substr(pos + len);
				if (with.find(replace) != std::string::npos) // If the replace containes the key look after the replace!
					pos = string.find(replace, pos + with.length());
				else
					pos = string.find(replace, pos + 1);
			}
		}

		inline std::string rpad(std::string str, std::size_t len) {
			if (str.length() > len)
				return str.substr(str.length() - len);
			return std::string(len - str.length(), ' ') + str;
		}
		inline std::string lpad(std::string str, std::size_t len) {
			if (str.length() > len)
				return str.substr(0, len);
			return str + std::string(len - str.length(), ' ');
		}

	}
}
