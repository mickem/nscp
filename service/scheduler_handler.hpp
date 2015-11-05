#pragma once

#include "task_scheduler.hpp"

namespace task_scheduler {
	struct schedule_metadata {
		enum task_source {
			MODULE,
			SETTINGS,
			METRICS
		};
		int plugin_id;
		task_source source;
		std::string info;
		std::string schedule;
	};

	struct scheduler : public schedule_handler {
		typedef boost::unordered_map<int, schedule_metadata> metadata_map;
		metadata_map metadata;
		simple_scheduler tasks;

		schedule_metadata get(int id);
		void handle_plugin(const schedule_metadata &metadata);
		void handle_settings();
		void handle_metrics();

		void start();
		void stop();

		void add_task(const schedule_metadata::task_source source, const std::string interval, const std::string info = "");

		virtual bool handle_schedule(scheduled_task item);
		virtual void on_error(std::string error);
		virtual void on_trace(std::string error);
	};
}