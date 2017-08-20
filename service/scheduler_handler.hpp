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
		void set_threads(int count);
	};
}