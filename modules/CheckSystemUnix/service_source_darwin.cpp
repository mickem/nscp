// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_service and the service-tags on macOS: launchd jobs in the system
// domain. There is no public C API that enumerates launchd jobs, so this runs
// launchctl - by absolute path, under the same deadline systemctl gets - and
// parses it in check_service_launchd.cpp. A daemon sees the system domain,
// which is the counterpart of systemd's system services.
//
// The main process's metrics come from libproc, readable for the agent's own
// processes and, as root, for all of them.

#include <libproc.h>
#include <sys/proc_info.h>

#include <cstring>
#include <ctime>
#include <set>
#include <string>
#include <vector>

#include "check_service.h"
#include "exec_command.h"
#include "mach_stats_darwin.h"

namespace checks {
namespace check_svc_filter {

namespace {

const char *const launchctl = "/bin/launchctl";

std::map<std::string, bool> read_disabled() { return parse_launchctl_disabled(system_exec::exec_command({launchctl, "print-disabled", "system"})); }

std::vector<launchd_listing> read_listing() { return parse_launchctl_services(system_exec::exec_command({launchctl, "print", "system"})); }

void fill_metrics(filter_obj &info) {
  if (info.pid <= 0) return;
  struct proc_bsdinfo bsd;
  std::memset(&bsd, 0, sizeof(bsd));
  if (proc_pidinfo(info.pid, PROC_PIDTBSDINFO, 0, &bsd, PROC_PIDTBSDINFO_SIZE) != PROC_PIDTBSDINFO_SIZE) return;
  const long long now = static_cast<long long>(::time(nullptr));
  info.created = static_cast<long long>(bsd.pbi_start_tvsec);
  info.age = now > info.created ? now - info.created : 0;

  struct proc_taskinfo task;
  std::memset(&task, 0, sizeof(task));
  if (proc_pidinfo(info.pid, PROC_PIDTASKINFO, 0, &task, PROC_PIDTASKINFO_SIZE) != PROC_PIDTASKINFO_SIZE) return;
  info.rss = static_cast<long long>(task.pti_resident_size);
  info.vms = static_cast<long long>(task.pti_virtual_size);
  info.tasks = task.pti_threadnum;
  // Lifetime average, as on Linux: CPU seconds over wall-clock seconds.
  const double cpu_secs = static_cast<double>(mach_stats::mach_ticks_to_ns(task.pti_total_user) + mach_stats::mach_ticks_to_ns(task.pti_total_system)) / 1e9;
  info.cpu = info.age > 0 ? cpu_secs / static_cast<double>(info.age) * 100.0 : 0.0;
  info.has_metrics = true;
}

filter_obj not_found(const std::string &name) {
  filter_obj info;
  info.name = name;
  info.load_state = "not-found";
  info.active = "inactive";
  info.sub_state = "dead";
  info.state = "stopped";
  return info;
}

}  // namespace

bool is_unit_active(const std::string &unit) { return !active_units({unit}).empty(); }

std::set<std::string> active_units(const std::vector<std::string> &units) {
  std::set<std::string> running;
  for (const launchd_listing &job : read_listing()) {
    if (job.pid > 0) running.insert(job.label);
  }
  std::set<std::string> active;
  for (const std::string &u : units) {
    if (running.count(u)) active.insert(u);
  }
  return active;
}

filter_obj get_service_info(const std::string &service) {
  if (!is_safe_unit_name(service)) return not_found(service);
  const std::map<std::string, std::string> properties = parse_launchctl_print(system_exec::exec_command({launchctl, "print", "system/" + service}));
  if (properties.empty()) return not_found(service);
  launchd_listing job;
  job.label = service;
  filter_obj info = launchd_row(job, read_disabled(), properties);
  fill_metrics(info);
  return info;
}

std::vector<filter_obj> enumerate_services(const std::string &state_filter) {
  std::vector<filter_obj> result;
  const std::map<std::string, bool> disabled = read_disabled();
  for (const launchd_listing &job : read_listing()) {
    // The listing alone, not a `launchctl print` per job: a Mac has several
    // hundred of them. That leaves start_type disabled or unknown here, and a
    // non-zero exit code is not taken as a failure (only a crash is); both
    // need the job's own properties, which a check by name reads.
    filter_obj info = launchd_row(job, disabled, {});
    if (state_filter == "active" && info.active != "active") continue;
    if (state_filter == "inactive" && info.active != "inactive") continue;
    if (state_filter == "failed" && info.active != "failed") continue;
    fill_metrics(info);
    result.push_back(info);
  }
  return result;
}

}  // namespace check_svc_filter
}  // namespace checks
