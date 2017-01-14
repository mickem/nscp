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

#include <boost/date_time.hpp>

class simple_timer {
	boost::posix_time::ptime start_time;
	std::string text;
	bool log;
public:
	simple_timer() {
		start();
	}
	simple_timer(std::string text, bool log) : text(text), log(log) {
		start();
	}
	~simple_timer() {
		if (log)
			std::cout << text << stop() << std::endl;;
	}

	void start() {
		start_time = get();
	}
	unsigned long long stop() {
		boost::posix_time::time_duration diff = get() - start_time;
		start();
		return diff.total_seconds();
	}

private:
	boost::posix_time::ptime get() {
		return boost::posix_time::microsec_clock::local_time();
	}

};