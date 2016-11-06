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
	bool process_item(filter_type &filter, transient_data_type);
	void set_split(std::string line, std::string column);

	void add_file(const boost::filesystem::path &path);
};