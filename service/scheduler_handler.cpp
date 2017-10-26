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

#include "scheduler_handler.hpp"
#include "NSClient++.h"
#include "../libs/settings_manager/settings_manager_impl.h"

#include <nsclient/logger/logger.hpp>

#include <str/format.hpp>


extern NSClient *mainClient;

namespace task_scheduler {
	schedule_metadata scheduler::get(int id) {
		boost::mutex::scoped_lock l(tasks.get_mutex());
		return metadata[id];
	}
	void scheduler::handle_plugin(const schedule_metadata &data) {
		nsclient::core::plugin_manager::plugin_type plugin = mainClient->get_plugin_manager()->find_plugin(data.plugin_id);
		plugin->handle_schedule("");
	}
	void scheduler::handle_reload(const schedule_metadata &data) {
		mainClient->do_reload(data.info);
	}
	void scheduler::handle_settings() {
		settings_manager::get_core()->house_keeping();
		if (settings_manager::get_core()->needs_reload()) {
			mainClient->reload("delayed,service");
		}
	}
	void scheduler::handle_metrics() {
		mainClient->process_metrics();
	}

	void scheduler::start() {
		tasks.set_handler(this);
		tasks.start();
	}
	void scheduler::stop() {
		tasks.stop();
		tasks.unset_handler();
	}

	boost::posix_time::seconds parse_interval(const std::string &str) {
		if (str.empty())
			return boost::posix_time::seconds(0);
		return boost::posix_time::seconds(str::format::stox_as_time_sec<long>(str, "s"));
	}

	void scheduler::add_task(schedule_metadata::task_source source, std::string interval, const std::string info) {
		unsigned int id = tasks.add_task("internal", parse_interval(interval), 0.5);
		schedule_metadata data;
		data.source = source;
		data.info = info;
		metadata[id] = data;
	}

	bool scheduler::handle_schedule(simple_scheduler::task item) {
		schedule_metadata metadata = get(item.id);
		if (metadata.source == schedule_metadata::MODULE) {
			handle_plugin(metadata);
			return true;
		} else if (metadata.source == schedule_metadata::SETTINGS) {
			handle_settings();
			return true;
		} else if (metadata.source == schedule_metadata::METRICS) {
			handle_metrics();
			return true;
		} else if (metadata.source == schedule_metadata::RELOAD) {
			handle_reload(metadata);
			return false;
		} else {
			on_error(__FILE__, __LINE__, "Unknown source");
			return false;
		}
	}

	void scheduler::on_error(const char* file, int line, std::string error) {
		mainClient->get_logger()->error("core::scheduler", file, line, error);
	}
	void scheduler::on_trace(const char* file, int line, std::string error) {
		mainClient->get_logger()->trace("core::scheduler", file, line, error);
	}

	void scheduler::set_threads(int count) {
		tasks.set_threads(count);
	}

}