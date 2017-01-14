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

//#define BOOST_SPIRIT_DEBUG  ///$$$ DEFINE THIS WHEN DEBUGGING $$$///
#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <boost/tuple/tuple.hpp>

namespace parsers {
	struct simple_expression {
		struct entry {
			bool is_variable;
			std::string name;
			entry() : is_variable(false) {}
			entry(bool is_variable, std::string name) : is_variable(is_variable), name(name) {}
			entry(bool is_variable, std::vector<char> name) : is_variable(is_variable), name(name.begin(), name.end()) {}
			entry(const entry &other) : is_variable(other.is_variable), name(other.name) {}
			const entry& operator= (const entry &other) {
				is_variable = other.is_variable;
				name = other.name;
				return *this;
			}
		};
		typedef std::vector<entry> result_type;
		static bool parse(const std::string &str, result_type& v);
	};
}