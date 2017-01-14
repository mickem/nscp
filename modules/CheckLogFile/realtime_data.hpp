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

#include <boost/filesystem/path.hpp>

#include "filter.hpp"

struct runtime_data {
	struct transient_data_impl {
		std::string path_;
		transient_data_impl(std::string path) : path_(path) {}
		std::string to_string() const { return path_; }
	};
	typedef logfile_filter::filter filter_type;
	typedef boost::shared_ptr<transient_data_impl> transient_data_type;

	struct file_container {
		boost::filesystem::path file;
		boost::uintmax_t size;
	};

	std::list<file_container> files;
	std::string column_split;
	std::string line_split;

	void boot() {}
	void touch(boost::posix_time::ptime now);
	bool has_changed(transient_data_type) const;
	modern_filter::match_result process_item(filter_type &filter, transient_data_type);
	void set_split(std::string line, std::string column);

	void add_file(const boost::filesystem::path &path);
};