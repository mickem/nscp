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

#include <str/format.hpp>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

namespace check_cpu_filter {
	void runtime_data::add(const std::string &time) {
		container c;
		c.alias = time;
		c.time = str::format::decode_time<long>(time, 1);
		checks.push_back(c);
	}

	modern_filter::match_result runtime_data::process_item(filter_type &filter, transient_data_type thread) {
		modern_filter::match_result ret;
		BOOST_FOREACH(container &c, checks) {
			std::map<std::string, windows::system_info::load_entry> vals = thread->get_cpu_load(c.time);
			typedef std::map<std::string, windows::system_info::load_entry>::value_type vt;
			BOOST_FOREACH(vt v, vals) {
				boost::shared_ptr<check_cpu_filter::filter_obj> record(new check_cpu_filter::filter_obj(c.alias, v.first, v.second));
				ret.append(filter.match(record));
			}
		}
		return ret;
	}
}
