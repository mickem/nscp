#include "scheduler_handler.hpp"

#include <nsclient/logger.hpp>
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
			on_error("Unknown source");
			return false;
		}
	}
	void scheduler::on_error(std::string error) {
		nsclient::logging::logger::get_logger()->error("core::scheduler", __FILE__, __LINE__, error);
	}
	void scheduler::on_trace(std::string error) {
		nsclient::logging::logger::get_logger()->trace("core::scheduler", __FILE__, __LINE__, error);
	}
}