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
#include <list>
#include <set>
#include <map>

#include <unicode_char.hpp>

#include <NSCAPI.h>
#include <net/net.hpp>
#include <strEx.h>

namespace nscapi {
	namespace protobuf {
		namespace types {
	
			struct decoded_simple_command_data {
				std::string command;
				std::string target;
				std::list<std::string> args;
			};

			struct destination_container {
				std::string id;
				net::url address;
				std::string comment;
				std::set<std::string> tags;
				typedef std::map<std::string,std::string> data_map;
				data_map data;
				void set_host(std::string value) {
					address.host = value;
				}
				void set_address(std::string value) {
					address = net::parse(value);
				}
				void set_port(std::string value) {
					address.port = strEx::s::stox<unsigned int>(value);
				}
				std::string get_protocol() const {
					return address.protocol;
				}
				bool has_protocol() const {
					return !address.protocol.empty();
				}

				static bool to_bool(std::string value, bool def = false) {
					if (value.empty())
						return def;
					if (value == "true" || value == "1" || value == "True")
						return true;
					return false;
				}
				static int to_int(std::string value, int def = 0) {
					if (value.empty())
						return def;
					try {
						return boost::lexical_cast<int>(value);
					} catch (...) {
						return def;
					}
				}

				inline int get_int_data(std::string key, int def = 0) {
					return to_int(data[key], def);
				}
				inline bool get_bool_data(std::string key, bool def = false) {
					return to_bool(data[key], def);
				}
				inline std::string get_string_data(std::string key, std::string def = "") {
					data_map::iterator it = data.find(key);
					if (it == data.end())
						return def;
					return it->second;
				}
				inline bool has_data(std::string key) {
					return data.find(key) != data.end();
				}

				void set_string_data(std::string key, std::string value) {
					if (key == "host")
						set_host(value);
					else 
						data[key] = value;
				}
				void set_int_data(std::string key, int value) {
					if (key == "port")
						address.port = value;
					else 
						data[key] = boost::lexical_cast<std::string>(value);
				}
				void set_bool_data(std::string key, bool value) {
					data[key] = value?"true":"false";
				}

				std::string to_string();
				void import(const destination_container &other);
				void apply(const destination_container &other);
			};
		}
	}
}
