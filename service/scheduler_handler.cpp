#include "scheduler_handler.hpp"

#include <nsclient/logger.hpp>
#include "../helpers/settings_manager/settings_manager_impl.h"

#include "NSClient++.h"

extern NSClient mainClient;

namespace task_scheduler {

	schedule_metadata scheduler::get(int id) {
		boost::mutex::scoped_lock l(tasks.get_mutex());
		return metadata[id];
	}
	void scheduler::handle_plugin(const schedule_metadata &metadata) {
		NSClientT::plugin_type plugin = mainClient.find_plugin(metadata.plugin_id);
		plugin->handle_schedule("");
	}
	void scheduler::handle_settings() {
		settings_manager::get_core()->house_keeping();
		if (settings_manager::get_core()->needs_reload()) {
			mainClient.reload("delayed,service");
		}
	}

	void scheduler::start() {
		tasks.set_handler(this);
		tasks.start();
	}
	void scheduler::stop() {
		tasks.set_handler(NULL);
		tasks.stop();
	}

	boost::posix_time::seconds parse_interval(const std::string &str) {
		return boost::posix_time::seconds(strEx::stoui_as_time_sec(str, 1));
	} 

	void scheduler::add_task(schedule_metadata::task_source source, std::string interval) {
		unsigned int id = tasks.add_task("internal", parse_interval(interval));
		schedule_metadata data;
		data.source = source;
		metadata[id] = data;
	}


	void scheduler::handle_schedule(scheduled_task item) {
		schedule_metadata metadata = get(item.id);
		if (metadata.source == schedule_metadata::MODULE) {
			handle_plugin(metadata);
		} else if (metadata.source == schedule_metadata::SETTINGS) {
			handle_settings();
		} else {
			on_error("Unknown source");
		}
	}
	void scheduler::on_error(std::string error) {
		nsclient::logging::logger::get_logger()->error("scheduler", __FILE__, __LINE__, error);
	}

}
