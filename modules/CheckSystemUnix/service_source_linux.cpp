// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_service and the service-tags on Linux: systemd units through
// `systemctl show`, and the main process's metrics from /proc.

#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check_service.h"
#include "exec_command.h"

namespace checks {
namespace check_svc_filter {

namespace {
std::string read_file(const std::string &path) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) return "";
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

// System-wide timing needed to turn a process' jiffies into wall-clock values.
struct sys_timing {
  long long btime;    // boot time (unix seconds)
  double uptime;      // seconds since boot
  long long hz;       // clock ticks per second
  long long now;      // current unix time
};

sys_timing read_sys_timing() {
  sys_timing t;
  t.hz = sysconf(_SC_CLK_TCK);
  if (t.hz <= 0) t.hz = 100;
  t.now = static_cast<long long>(::time(nullptr));
  t.btime = 0;
  {
    std::istringstream iss(read_file("/proc/stat"));
    std::string line;
    while (std::getline(iss, line)) {
      if (line.compare(0, 6, "btime ") == 0) {
        try {
          t.btime = std::stoll(line.substr(6));
        } catch (...) {
        }
        break;
      }
    }
  }
  t.uptime = 0;
  {
    std::istringstream iss(read_file("/proc/uptime"));
    iss >> t.uptime;
  }
  return t;
}

proc_metrics read_proc_metrics(long long pid, const sys_timing &timing) {
  proc_metrics m;
  if (pid <= 0) return m;
  const std::string base = "/proc/" + std::to_string(pid);
  parse_status_mem(read_file(base + "/status"), m.rss, m.vms);
  unsigned long long utime = 0, stime = 0, starttime = 0;
  if (parse_stat_times(read_file(base + "/stat"), utime, stime, starttime)) {
    m.cpu = compute_cpu_pct(utime, stime, starttime, timing.uptime, timing.hz);
    if (timing.btime > 0 && timing.hz > 0) {
      m.created = timing.btime + static_cast<long long>(starttime / timing.hz);
      m.age = timing.now - m.created;
      if (m.age < 0) m.age = 0;
    }
  }
  m.valid = true;
  return m;
}

// Populate per-process metrics for a running service.
void fill_metrics(filter_obj &info, const sys_timing &timing) {
  if (info.state != "running" || info.pid <= 0) return;
  const proc_metrics m = read_proc_metrics(info.pid, timing);
  if (!m.valid) return;
  info.rss = m.rss;
  info.vms = m.vms;
  info.cpu = m.cpu;
  info.created = m.created;
  info.age = m.age;
  info.has_metrics = true;
}

}  // namespace

bool is_unit_active(const std::string &unit) {
  if (!is_safe_unit_name(unit)) return false;
  const std::vector<filter_obj> parsed = parse_systemctl_show(system_exec::exec_command({"systemctl", "show", "--no-pager", "--", unit}));
  if (parsed.empty()) return false;
  // A missing unit still yields a block (LoadState=not-found) with
  // ActiveState=inactive, so "started" covers existence too.
  return parsed.front().is_started();
}

std::set<std::string> active_units(const std::vector<std::string> &units) {
  std::set<std::string> active;
  // One bulk `systemctl show -- u1 u2 ...` instead of a fork per unit;
  // parse_systemctl_show already returns a block per unit, in argument order.
  std::vector<std::string> safe;
  for (const std::string &u : units) {
    if (is_safe_unit_name(u)) safe.push_back(u);
  }
  if (safe.empty()) return active;
  std::vector<std::string> argv = {"systemctl", "show", "--no-pager", "--"};
  argv.insert(argv.end(), safe.begin(), safe.end());
  const std::vector<filter_obj> parsed = parse_systemctl_show(system_exec::exec_command(argv));

  // filter_obj::name is the Id with the .service suffix stripped; index the
  // started ones by that canonical name.
  std::set<std::string> started;
  for (const filter_obj &info : parsed) {
    if (info.is_started()) started.insert(info.name);
  }
  // Return the caller's original spellings, matching with or without .service.
  for (const std::string &u : units) {
    std::string canonical = u;
    if (boost::ends_with(canonical, ".service")) canonical = canonical.substr(0, canonical.size() - 8);
    if (started.count(canonical)) active.insert(u);
  }
  return active;
}

namespace {

// Get one service's info via `systemctl show`, then attach process metrics.
filter_obj read_service_info(const std::string &service_name, const sys_timing &timing) {
  filter_obj info;
  info.name = service_name;

  if (!is_safe_unit_name(service_name)) {
    return info;
  }

  const std::vector<filter_obj> parsed = parse_systemctl_show(system_exec::exec_command({"systemctl", "show", "--no-pager", "--", service_name}));
  if (!parsed.empty()) info = parsed.front();

  // Fall back to is-enabled when the show output lacked UnitFileState.
  if (info.start_type.empty()) {
    std::string enabled_output = system_exec::exec_command({"systemctl", "is-enabled", "--", service_name});
    boost::trim(enabled_output);
    if (!enabled_output.empty()) info.start_type = enabled_output;
  }

  fill_metrics(info, timing);
  return info;
}

}  // namespace

filter_obj get_service_info(const std::string &service) {
  // Add .service suffix if not present
  std::string service_name = service;
  if (!boost::ends_with(service_name, ".service")) {
    service_name += ".service";
  }

  filter_obj info = read_service_info(service_name, read_sys_timing());

  // Remove .service suffix for display
  if (boost::ends_with(info.name, ".service")) {
    info.name = info.name.substr(0, info.name.length() - 8);
  }
  return info;
}

namespace {

// List all service unit names via systemctl list-units.
std::vector<std::string> list_service_units() {
  std::vector<std::string> names;
  const std::string output = system_exec::exec_command({"systemctl", "list-units", "--type=service", "--all", "--no-legend", "--plain", "--no-pager"});
  std::istringstream iss(output);
  std::string line;
  while (std::getline(iss, line)) {
    std::istringstream ls(line);
    std::string tok;
    while (ls >> tok) {
      if (boost::ends_with(tok, ".service") && is_safe_unit_name(tok)) {
        names.push_back(tok);
        break;
      }
    }
  }
  return names;
}

}  // namespace

// Enumerate all services: one bulk `systemctl show` for every unit, then
// process metrics from /proc (no extra forks per service).
std::vector<filter_obj> enumerate_services(const std::string &state_filter) {
  const sys_timing timing = read_sys_timing();
  std::vector<filter_obj> result;
  const std::vector<std::string> names = list_service_units();
  if (names.empty()) return result;

  std::vector<std::string> argv = {"systemctl", "show", "--no-pager", "--"};
  argv.insert(argv.end(), names.begin(), names.end());
  std::vector<filter_obj> parsed = parse_systemctl_show(system_exec::exec_command(argv));

  for (filter_obj &info : parsed) {
    if (state_filter == "active" && info.active != "active") continue;
    if (state_filter == "inactive" && info.active != "inactive") continue;
    if (state_filter == "failed" && info.active != "failed") continue;
    fill_metrics(info, timing);
    result.push_back(info);
  }
  return result;
}

}  // namespace check_svc_filter
}  // namespace checks
