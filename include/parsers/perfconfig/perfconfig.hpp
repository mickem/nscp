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

#define BOOST_SPIRIT_DEBUG  ///$$$ DEFINE THIS WHEN DEBUGGING $$$///
#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <boost/tuple/tuple.hpp>

namespace parsers {
	struct perfconfig {
		struct perf_option {
			std::string key;
			std::string value;
		};
		struct perf_rule {
			std::string name;
			std::vector<perf_option> options;
		};
		typedef std::vector<perf_rule> result_type;
		bool parse(const std::string &str, result_type& v);
	};
}