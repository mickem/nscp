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

#include <list>
#include <pdh.h>
#include <pdhmsg.h>
#include <sstream>
#include <error/error.hpp>

namespace PDH {
#define PDH_INDEX_BUF_LEN 2048

	class Enumerations {
	public:
		struct Object {
			std::string name;
			std::string error;
			std::list<std::string> instances;
			std::list<std::string> counters;
		};

		typedef std::list<Object> Objects;
		static std::list<std::string> expand_wild_card_path(const std::string &query, std::string &error);
		static void fetch_object_details(Object &object, bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD);
		static Objects EnumObjects(bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD);
		static Object EnumObject(std::string object, bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD);
	};
}
