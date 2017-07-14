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
#include "filter.hpp"

#include <list>

struct runtime_data {
	typedef eventlog_filter::filter filter_type;
	typedef eventlog_filter::filter::object_type transient_data_type;

	int truncate_;
	std::list<std::string> files;

	runtime_data() : truncate_(0) {}
	runtime_data(int truncate) : truncate_(truncate) {}
	void boot() {}
	void touch(boost::posix_time::ptime) {}
	bool has_changed(transient_data_type record) const;
	modern_filter::match_result process_item(filter_type &filter, transient_data_type data);
	void add_file(const std::string &file);
};