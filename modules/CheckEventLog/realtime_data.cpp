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

#include "realtime_data.hpp"

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <str/xtos.hpp>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

bool runtime_data::has_changed(transient_data_type record) const {
	if (files.empty())
		return true;
	std::string log_lc = boost::to_lower_copy(record->get_log());
	BOOST_FOREACH(const std::string &s, files) {
		if (s == "any" || s == "all" || s == log_lc)
			return true;
	}
	return false;
}

void runtime_data::add_file(const std::string &file) {
	files.push_back(boost::to_lower_copy(file));
}

modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type record) {
	return filter.match(record);
}