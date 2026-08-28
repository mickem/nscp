// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "schedules_handler.hpp"

namespace schedules {

target_object scheduler::get(int id) {
  boost::mutex::scoped_lock l(tasks.get_mutex());
  return metadata[id];
}

std::list<target_object> scheduler::get_all() {
  std::list<target_object> ret;
  boost::mutex::scoped_lock l(tasks.get_mutex());
  for (const metadata_map::value_type &v : metadata) {
    if (v.second) ret.push_back(v.second);
  }
  return ret;
}

void scheduler::start() {
  tasks.set_handler(this);
  tasks.start();
}
void scheduler::stop() {
  // tasks.stop() joins the worker threads, so nothing can call back into this
  // object afterwards. Both handler pointers are cleared only once that join
  // has returned - clearing them earlier (which is what the callers used to do
  // via a separate unset_handler() before stop()) left a window where a worker
  // had already passed its null check and was about to call into an object the
  // unload path was destroying.
  tasks.stop();
  tasks.unset_handler();
  handler_ = nullptr;
}

void scheduler::clear() {
  tasks.clear_tasks();
  {
    boost::mutex::scoped_lock l(tasks.get_mutex());
    metadata.clear();
  }
}

boost::posix_time::seconds parse_interval(const std::string &str) {
  if (str.empty()) return boost::posix_time::seconds(0);
  return boost::posix_time::seconds(str::format::stox_as_time_sec<long>(str, "s"));
}

void scheduler::add_task(const target_object target) {
  // A run-on-startup task is not queued here: run_startup_tasks() queues its
  // first instance once the agent is up. Queueing it now as well would leave
  // the task with two independent instances (each run queues the next one),
  // making it fire twice per interval for the lifetime of the process.
  const bool schedule_first_run = !target->run_on_startup;
  unsigned int id = 0;
  if (target->duration)
    id = tasks.add_task(target->get_alias(), *target->duration, target->randomness, schedule_first_run);
  else if (target->schedule)
    id = tasks.add_task(target->get_alias(), cron_parser::parse(*target->schedule), schedule_first_run);
  else
    id = tasks.add_task(target->get_alias(), parse_interval("5m"), 0.1, schedule_first_run);
  {
    boost::mutex::scoped_lock l(tasks.get_mutex());
    metadata[id] = target;
  }
}

void scheduler::run_startup_tasks(const boost::posix_time::time_duration window) {
  std::list<int> ids;
  {
    boost::mutex::scoped_lock l(tasks.get_mutex());
    for (const metadata_map::value_type &v : metadata) {
      if (v.second && v.second->run_on_startup) ids.push_back(v.first);
    }
  }
  if (ids.empty()) return;
  // Evenly spaced over the window: the first task runs immediately and no task
  // waits the full window (with a zero window every offset collapses to 0).
  const long step = window.total_milliseconds() / static_cast<long>(ids.size());
  long offset = 0;
  for (const int id : ids) {
    tasks.run_now(id, boost::posix_time::milliseconds(offset));
    offset += step;
  }
}

}  // namespace schedules