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