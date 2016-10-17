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

#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>

#include <file_helpers.hpp>

#include "filter.hpp"

namespace file_finder {
	bool is_directory(unsigned long dwAttr);

	struct scanner_context {
		bool debug;
		std::string pattern;
		DWORD now;
		int max_depth;
		bool is_valid_level(int current_level);
		void report_error(const std::string str);
		void report_debug(const std::string str);
		void report_warning(const std::string msg);
	};

	void recursive_scan(file_filter::filter &filter, scanner_context &context, boost::filesystem::path dir, boost::shared_ptr<file_filter::filter_obj> total_obj, bool total_all, bool recursive = false, int current_level = 0);
}