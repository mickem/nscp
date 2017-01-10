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
