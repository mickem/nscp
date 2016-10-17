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

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <strEx.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace check_cpu_filter {
	void runtime_data::add(const std::string &time) {
		container c;
		c.alias = time;
		c.time = format::decode_time<long>(time, 1);
		checks.push_back(c);
	}

	bool runtime_data::process_item(filter_type &filter, transient_data_type thread) {
		bool matched = false;
		BOOST_FOREACH(container &c, checks) {
			std::map<std::string, windows::system_info::load_entry> vals = thread->get_cpu_load(c.time);
			typedef std::map<std::string, windows::system_info::load_entry>::value_type vt;
			BOOST_FOREACH(vt v, vals) {
				boost::shared_ptr<check_cpu_filter::filter_obj> record(new check_cpu_filter::filter_obj(c.alias, v.first, v.second));
				modern_filter::match_result ret = filter.match(record);
				if (ret.matched_bound) {
					matched = true;
					if (ret.is_done) {
						break;
					}
				}
			}
		}
		return matched;
	}
}
