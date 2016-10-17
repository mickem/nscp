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

#include "scheduler_handler.hpp"

#include <nsclient/logger/logger.hpp>
#include "../libs/settings_manager/settings_manager_impl.h"

#include "NSClient++.h"

extern NSClient *mainClient;

namespace task_scheduler {
	schedule_metadata scheduler::get(int id) {
		boost::mutex::scoped_lock l(tasks.get_mutex());
		return metadata[id];
	}
	void scheduler::handle_plugin(const schedule_metadata &data) {
		NSClientT::plugin_type plugin = mainClient->find_plugin(data.plugin_id);
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
		return boost::posix_time::seconds(strEx::stoui_as_time_sec(str, 1));
	}

	void scheduler::add_task(schedule_metadata::task_source source, std::string interval, const std::string info) {
		unsigned int id = tasks.add_task("internal", parse_interval(interval));
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
}