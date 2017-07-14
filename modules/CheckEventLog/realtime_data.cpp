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
	record->set_truncate(truncate_);
	return filter.match(record);
}