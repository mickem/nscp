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
	typedef eventlog_filter::filter filter_type;
	typedef eventlog_filter::filter::object_type transient_data_type;

	std::list<std::string> files;

	void boot() {}
	void touch(boost::posix_time::ptime) {}
	bool has_changed(transient_data_type record) const;
	bool process_item(filter_type &filter, transient_data_type data);
	void add_file(const std::string &file);
};