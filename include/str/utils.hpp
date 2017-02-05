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

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include <list>
#include <string>

namespace str {
	namespace utils {
		//
		// Replace
		//
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

		//
		// Split
		//
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
		typedef std::pair<std::string, std::string> token;
		inline token split2(const std::string str, const std::string key) {
			std::string::size_type pos = str.find(key);
			if (pos == std::string::npos)
				return token(str, std::string());
			return token(str.substr(0, pos), str.substr(pos + key.length()));
		}
		inline std::list<std::string> split_lst(const std::string str, const std::string key) {
			std::list<std::string> ret;
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

		//
		// Tokenizer
		//
		inline token getToken(std::string buffer, char split) {
			std::string::size_type pos = std::string::npos;
			pos = buffer.find(split);
			if (pos == std::string::npos)
				return token(buffer, "");
			if (pos == buffer.length() - 1)
				return token(buffer.substr(0, pos), "");
			return token(buffer.substr(0, pos), buffer.substr(pos + 1));
		}

		//
		// Parsing commands
		//
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
		inline void parse_command(std::string cmd_line, std::string &cmd, std::list<std::string> &args) {
			boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string > tok(cmd_line, boost::escaped_list_separator<char>('\\', ' ', '\"'));
			bool first = true;
			BOOST_FOREACH(std::string s, tok) {
				if (first) {
					cmd = s;
					first = false;
				} else {
					args.push_back(s);
				}
			}
		}
	}
}
