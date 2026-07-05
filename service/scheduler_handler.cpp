// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "scheduler_handler.hpp"

#include <nsclient/logger/logger.hpp>
#include <str/format.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"
#include "NSClient++.h"

extern std::shared_ptr<NSClient> mainClient;

namespace task_scheduler {
schedule_metadata scheduler::get(int id) {
  boost::mutex::scoped_lock l(tasks.get_mutex());
  return metadata[id];
}
void scheduler::handle_plugin(const schedule_metadata &data) {
  nsclient::core::plugin_manager::plugin_type plugin = mainClient->get_plugin_manager()->find_plugin(data.plugin_id);
  plugin->handle_schedule("");
}
void scheduler::handle_reload(const schedule_metadata &data) { mainClient->do_reload(data.info); }
void scheduler::handle_settings() {
  settings_manager::get_core()->house_keeping();
  if (settings_manager::get_core()->needs_reload()) {
    mainClient->reload("delayed,service");
  }
}
void scheduler::handle_metrics() { mainClient->process_metrics(); }

void scheduler::start() {
  tasks.set_handler(this);
  tasks.start();
}
void scheduler::stop() {
  tasks.stop();
  tasks.unset_handler();
}

boost::posix_time::seconds parse_interval(const std::string &str) {
  if (str.empty()) return boost::posix_time::seconds(0);
  return boost::posix_time::seconds(str::format::stox_as_time_sec<long>(str, "s"));
}

void scheduler::add_task(schedule_metadata::task_source source, std::string interval, const std::string info) {
  const auto metrics_interval = parse_interval(interval);
  if (source == schedule_metadata::METRICS) {
    metrics_interval_ = static_cast<unsigned int>(metrics_interval.total_seconds());
  }
  unsigned int id = tasks.add_task("internal", metrics_interval, 0.5);
  schedule_metadata data;
  data.source = source;
  data.info = info;
  metadata[id] = data;
}

bool scheduler::handle_schedule(simple_scheduler::task item) {
  schedule_metadata current_metadata = get(item.id);
  if (current_metadata.source == schedule_metadata::MODULE) {
    handle_plugin(current_metadata);
    return true;
  } else if (current_metadata.source == schedule_metadata::SETTINGS) {
    handle_settings();
    return true;
  } else if (current_metadata.source == schedule_metadata::METRICS) {
    handle_metrics();
    return true;
  } else if (current_metadata.source == schedule_metadata::RELOAD) {
    handle_reload(current_metadata);
    return false;
  } else {
    on_error(__FILE__, __LINE__, "Unknown source");
    return false;
  }
}

void scheduler::on_error(const char *file, int line, std::string error) { mainClient->get_logger()->error("core::scheduler", file, line, error); }
void scheduler::on_trace(const char *file, int line, std::string error) { mainClient->get_logger()->trace("core::scheduler", file, line, error); }

void scheduler::set_threads(int count) { tasks.set_threads(count); }

}  // namespace task_scheduler