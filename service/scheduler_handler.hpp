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

#include <scheduler/simple_scheduler.hpp>



namespace task_scheduler {
	struct schedule_metadata {
		enum task_source {
			MODULE,
			SETTINGS,
			METRICS,
			RELOAD
		};
		int plugin_id;
		task_source source;
		std::string info;
		std::string schedule;
	};

	struct scheduler : public simple_scheduler::handler {
		typedef boost::unordered_map<int, schedule_metadata> metadata_map;
		metadata_map metadata;
		simple_scheduler::scheduler tasks;

		schedule_metadata get(int id);
		void handle_plugin(const schedule_metadata &metadata);
		void handle_reload(const schedule_metadata &metadata);
		void handle_settings();
		void handle_metrics();

		const simple_scheduler::scheduler& get_scheduler() {
			return tasks;
		}

		void start();
		void stop();

		void add_task(const schedule_metadata::task_source source, const std::string interval, const std::string info = "");

		bool handle_schedule(simple_scheduler::task item);
		virtual void on_error(const char* file, int line, std::string error);
		virtual void on_trace(const char* file, int line, std::string error);
	};
}