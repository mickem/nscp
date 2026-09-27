// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

namespace task_facts {
extern const char *const set_tasks;
extern const char *const id_scheduled;
extern const std::size_t max_tasks;

struct task {
  std::string id;  // Full registered path, the check's uri (not its title).
  std::string name;
  std::string folder;
  bool enabled = false;
  // Unknown on the legacy Task Scheduler API; omitted there.
  int hidden = -1;
};
void publish(std::vector<task> tasks, std::time_t taken_at, nscapi::facts::response &out);
std::vector<task> gather();
}  // namespace task_facts
