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

#include <map>
#include <string>
#include <nscapi/nscapi_protobuf.hpp>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/mutex.hpp>

namespace metrics {

	struct metrics_store {
		typedef std::map<std::string, std::string> values_map;
		void set(const Plugin::MetricsMessage &response);
		values_map get(const std::string &filter);
	private:
		values_map values_;
		boost::timed_mutex mutex_;
	};

}
