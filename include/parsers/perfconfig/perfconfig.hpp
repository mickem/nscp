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