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
