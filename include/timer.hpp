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

#include <str/xtos.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>


namespace pt = boost::posix_time;

struct timer {
	boost::posix_time::ptime start_time;
	std::list<std::string> times;
	std::string current;

	timer() : start_time(pt::microsec_clock::local_time()) {}

	void check(std::string tag) {
		pt::ptime time = pt::microsec_clock::local_time();
		pt::time_duration diff = time - start_time;
		times.push_back(tag + ": " + str::xtos(diff.total_milliseconds()));
	}
	void start(const std::string tag) {
		check(">>> " + tag);
		current = tag;
	}
	void end() {
		check("<<< " + current);
		current = "";
	}

	std::list<std::string> get() {
		check("DONE");
		return times; 
	}
};
	

