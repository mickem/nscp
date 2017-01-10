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

#include <nscapi/nscapi_plugin_impl.hpp>
#include <scheduler/simple_scheduler.hpp>
#include "schedules_handler.hpp"

typedef schedules::schedule_handler::object_instance schedule_instance;
class Scheduler : public schedules::task_handler, public nscapi::impl::simple_plugin {
private:

	schedules::scheduler scheduler_;
	schedules::schedule_handler schedules_;

public:
	Scheduler() {
		scheduler_.set_handler(this);
	}
	virtual ~Scheduler() {
		scheduler_.set_handler(NULL);
	}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	// Metrics
	void fetchMetrics(Plugin::MetricsMessage::Response *response);

	void add_schedule(std::string alias, std::string command);
	bool handle_schedule(schedules::target_object task);

	void on_error(const char* file, int line, std::string error);
	void on_trace(const char* file, int line, std::string error);
};