// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_cpu_utilization.h"

#include <chrono>
#include <fstream>
#include <locale>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <string>
#include <thread>

namespace cpu_utilization_check {

namespace {
// Read the whole of /proc/stat. Returns "" on failure.
std::string read_proc_stat() {
  std::ifstream ifs("/proc/stat");
  if (!ifs.is_open()) return "";
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

double pct(unsigned long long delta, unsigned long long total) {
  if (total == 0) return 0.0;
  return static_cast<double>(delta) * 100.0 / static_cast<double>(total);
}

// Cumulative-counter delta that is robust to a counter reset (returns 0).
unsigned long long sub(unsigned long long a, unsigned long long b) { return a >= b ? a - b : 0; }
}  // namespace

cpu_jiffies parse_proc_stat_cpu(const std::string &content) {
  cpu_jiffies out;
  std::istringstream lines(content);
  std::string line;
  while (std::getline(lines, line)) {
    if (line.compare(0, 4, "cpu ") != 0) continue;  // aggregate line only (note the trailing space)
    std::istringstream is(line);
    is.imbue(std::locale("C"));
    std::string label;
    is >> label >> out.user >> out.nice >> out.system >> out.idle;
    // iowait and beyond are optional on very old kernels; read what is present.
    is >> out.iowait >> out.irq >> out.softirq >> out.steal >> out.guest >> out.guest_nice;
    out.valid = true;
    return out;
  }
  return out;
}

util_obj compute_utilization(const cpu_jiffies &prev, const cpu_jiffies &cur) {
  util_obj u;
  const unsigned long long dt = sub(cur.total(), prev.total());
  if (dt == 0) return u;
  const double didle = static_cast<double>(sub(cur.idle, prev.idle) + sub(cur.iowait, prev.iowait));
  u.user = pct(sub(cur.user, prev.user) + sub(cur.nice, prev.nice), dt);
  u.system = pct(sub(cur.system, prev.system), dt);
  u.iowait = pct(sub(cur.iowait, prev.iowait), dt);
  u.irq = pct(sub(cur.irq, prev.irq), dt);
  u.softirq = pct(sub(cur.softirq, prev.softirq), dt);
  u.steal = pct(sub(cur.steal, prev.steal), dt);
  u.guest = pct(sub(cur.guest, prev.guest) + sub(cur.guest_nice, prev.guest_nice), dt);
  u.idle = pct(sub(cur.idle, prev.idle), dt);
  // total = everything that is not idle/iowait (i.e. real busy time). Using
  // 100-idle-iowait keeps a machine stuck in iowait from reading as 100% busy
  // while still surfacing iowait separately.
  u.total = 100.0 - pct(static_cast<unsigned long long>(didle), dt);
  if (u.total < 0) u.total = 0;
  return u;
}

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &util_obj::get_name, "Always 'total' (single aggregate row)");

  // Perf is emitted via the extra() perf-config; the default perf generator
  // names each metric "cpu_<keyword>" (e.g. cpu_total, cpu_iowait).
  registry_.add_float("total", &util_obj::get_total, "Non-idle CPU utilization in percent (100 - idle - iowait)");
  registry_.add_float("user", &util_obj::get_user, "User (incl. nice) CPU utilization in percent");
  registry_.add_float("system", &util_obj::get_system, "System/kernel CPU utilization in percent");
  registry_.add_float("iowait", &util_obj::get_iowait, "I/O-wait CPU utilization in percent");
  registry_.add_float("irq", &util_obj::get_irq, "Hardware-interrupt CPU utilization in percent");
  registry_.add_float("softirq", &util_obj::get_softirq, "Soft-interrupt CPU utilization in percent");
  registry_.add_float("steal", &util_obj::get_steal, "Stolen (hypervisor) CPU utilization in percent");
  registry_.add_float("guest", &util_obj::get_guest, "Guest (incl. guest_nice) CPU utilization in percent");
  registry_.add_float("idle", &util_obj::get_idle, "Idle CPU in percent");
}

void check_cpu_utilization_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                const cpu_jiffies &prev, const cpu_jiffies &cur) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("total > 90", "total > 95", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "user: ${user}% system: ${system}% iowait: ${iowait}% steal: ${steal}% idle: ${idle}%", "cpu", "", "");
  // Emit the full breakdown as perf data (not just the thresholded 'total').
  filter_helper.set_default_perf_config("extra(total;user;system;iowait;irq;softirq;steal;guest;idle)");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<util_obj> record(new util_obj(compute_utilization(prev, cur)));
  filter.match(record);

  filter_helper.post_process(filter);
}

void check_cpu_utilization(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  const cpu_jiffies prev = parse_proc_stat_cpu(read_proc_stat());
  if (!prev.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/stat");
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const cpu_jiffies cur = parse_proc_stat_cpu(read_proc_stat());
  if (!cur.valid) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read /proc/stat");
  }
  check_cpu_utilization_from(request, response, prev, cur);
}

}  // namespace cpu_utilization_check
