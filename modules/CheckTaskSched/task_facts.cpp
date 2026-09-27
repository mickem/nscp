// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "task_facts.hpp"

#include <nscapi/nscapi_facts_helper.hpp>
#include <stdexcept>

namespace task_facts {
const char *const set_tasks = "tasks";
const char *const id_scheduled = "tasks.scheduled";
const std::size_t max_tasks = 2500;

void publish(std::vector<task> tasks, const std::time_t taken_at, nscapi::facts::response &out) {
  std::sort(tasks.begin(), tasks.end(), [](const task &a, const task &b) { return a.id < b.id; });
  // Validate before writing any part of the set. A duplicate identity is a
  // failed inventory, not a reason to silently lose one of the tasks.
  for (std::size_t i = 0; i < tasks.size(); ++i) {
    if (tasks[i].id.empty() || (i > 0 && tasks[i].id == tasks[i - 1].id)) throw std::runtime_error("Missing or duplicate scheduled task identity");
  }
  nscapi::facts::record_list list = out.set(set_tasks).list("scheduled");
  for (std::size_t i = 0; i < std::min(tasks.size(), max_tasks); ++i) {
    const task &t = tasks[i];
    auto record = list.record(t.id);
    record.value("name", t.name).value("folder", t.folder).value("enabled", t.enabled);
    if (t.hidden >= 0) record.value("hidden", t.hidden != 0);
  }
  if (tasks.size() > max_tasks)
    out.error(set_tasks, "Scheduled task inventory truncated: found " + std::to_string(tasks.size()) + ", reporting " + std::to_string(max_tasks));
  out.gathered(set_tasks, taken_at);
}
}  // namespace task_facts
