// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <scheduler/simple_scheduler.hpp>

namespace task_scheduler {
struct schedule_metadata {
  // UNKNOWN is what a lookup returns for a task whose metadata has not been
  // published yet: add_task() registers the task with the scheduler before it
  // can fill in the metadata, so the worker can briefly see the id first. It
  // must be the default, or that window dispatches on an uninitialised value.
  enum task_source { UNKNOWN, MODULE, SETTINGS, METRICS, RELOAD };
  int plugin_id = 0;
  task_source source = UNKNOWN;
  std::string info;
  std::string schedule;
};

struct scheduler : public simple_scheduler::handler {
  typedef boost::unordered_map<int, schedule_metadata> metadata_map;
  metadata_map metadata;
  simple_scheduler::scheduler tasks;
  unsigned int metrics_interval_ = 0;

  schedule_metadata get(int id);
  void handle_plugin(const schedule_metadata& metadata);
  void handle_reload(const schedule_metadata& metadata);
  void handle_settings();
  void handle_metrics();

  const simple_scheduler::scheduler& get_scheduler() { return tasks; }

  void start();
  void stop();

  void add_task(const schedule_metadata::task_source source, const std::string interval, const std::string info = "");

  bool handle_schedule(simple_scheduler::task item);
  virtual void on_error(const char* file, int line, std::string error);
  virtual void on_trace(const char* file, int line, std::string error);
  void set_threads(int count);

  unsigned int get_metrics_interval() const { return metrics_interval_; }
};
}  // namespace task_scheduler