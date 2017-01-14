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
	

