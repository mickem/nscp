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

#include "schedules_handler.hpp"

namespace schedules {

	target_object scheduler::get(int id) {
		boost::mutex::scoped_lock l(tasks.get_mutex());
		return metadata[id];
	}

	void scheduler::start() {
		tasks.set_handler(this);
		tasks.start();
	}
	void scheduler::stop() {
		tasks.stop();
		tasks.unset_handler();
	}

	void scheduler::clear() {
		tasks.clear_tasks();
		{
			boost::mutex::scoped_lock l(tasks.get_mutex());
			metadata.clear();
		}
	}


	boost::posix_time::seconds parse_interval(const std::string &str) {
		if (str.empty())
			return boost::posix_time::seconds(0);
		return boost::posix_time::seconds(str::format::stox_as_time_sec<long>(str, "s"));
	}

	void scheduler::add_task(const target_object target) {
		unsigned int id = 0;
		if (target->duration)
			id = tasks.add_task(target->get_alias(), *target->duration, target->randomness);
		else if (target->schedule)
			id = tasks.add_task(target->get_alias(), cron_parser::parse(*target->schedule));
		else
			id = tasks.add_task(target->get_alias(), parse_interval("5m"), 0.1);
		{
			boost::mutex::scoped_lock l(tasks.get_mutex());
			metadata[id] = target;
		}
	}

}