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
