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
#include "pdh_thread.hpp"
#include "CheckMemory.h""

namespace check_cpu_filter {
	struct runtime_data {
		typedef check_cpu_filter::filter filter_type;
		typedef pdh_thread* transient_data_type;

		struct container {
			std::string alias;
			long time;
		};

		std::list<container> checks;

		void boot() {}
		void touch(boost::posix_time::ptime now) {}
		bool has_changed(transient_data_type) const { return true; }
		modern_filter::match_result process_item(filter_type &filter, transient_data_type);
		void add(const std::string &time);
	};
}
