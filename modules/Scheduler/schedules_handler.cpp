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
		return boost::posix_time::seconds(str::format::stox_as_time<long>(str, 1));
	}

	void scheduler::add_task(const target_object target) {
		unsigned int id = -1;
		if (target->duration)
			id = tasks.add_task(target->get_alias(), *target->duration);
		else if (target->schedule)
			id = tasks.add_task(target->get_alias(), cron_parser::parse(*target->schedule));
		else
			id = tasks.add_task(target->get_alias(), parse_interval("5m"));
		if (id == -1)
			return;
		{
			boost::mutex::scoped_lock l(tasks.get_mutex());
			metadata[id] = target;
		}
	}

}