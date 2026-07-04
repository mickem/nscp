#ifndef NSCP_CHECK_SERVICE_H
#define NSCP_CHECK_SERVICE_H
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace checks {

namespace check_svc_filter {

struct filter_obj {
  std::string name;
  std::string desc;
  std::string active;      // raw systemd ActiveState: active, inactive, failed
  std::string sub_state;   // raw systemd SubState: running, dead, exited, ...
  std::string load_state;  // loaded, not-found, masked, ...
  std::string state;       // stopped, starting, oneshot, running, static, unknown
  std::string start_type;  // UnitFileState: enabled, disabled, static, masked, ...
  std::string preset;      // UnitFilePreset: enabled, disabled
  int pid;

  // Per-MainPID process metrics (only populated for running services).
  bool has_metrics;
  long long rss;      // bytes
  long long vms;      // bytes
  double cpu;         // percent (lifetime average)
  long long tasks;    // cgroup task count
  long long created;  // unix timestamp of process start (0 if unknown)
  long long age;      // seconds since started (0 if unknown)

  filter_obj() : pid(0), has_metrics(false), rss(0), vms(0), cpu(0), tasks(0), created(0), age(0) {}

  filter_obj(const filter_obj &other) = default;

  std::string get_name() const { return name; }
  std::string get_desc() const { return desc; }
  std::string get_active() const { return active; }
  std::string get_state_s() const { return state; }
  std::string get_sub_state() const { return sub_state; }
  std::string get_load_state() const { return load_state; }
  std::string get_start_type_s() const { return start_type; }
  std::string get_preset() const { return preset; }
  long long get_pid() const { return pid; }
  long long get_rss() const { return rss; }
  long long get_vms() const { return vms; }
  double get_cpu() const { return cpu; }
  long long get_tasks() const { return tasks; }
  long long get_created() const { return created; }
  long long get_age() const { return age; }

  std::string show() const { return name + ":" + state; }

  static std::string map_state(const std::string &active_state, const std::string &sub, const std::string &unit_file_state) {
    const std::string combo = active_state + " (" + sub + ")";
    std::string mapped;
    if (combo == "inactive (dead)")
      mapped = "stopped";
    else if (combo == "activating (start)")
      mapped = "starting";
    else if (combo == "active (exited)")
      mapped = "oneshot";
    else if (combo == "active (running)")
      mapped = "running";
    else if (active_state == "failed")
      mapped = "stopped";
    else
      mapped = "unknown";
    if (unit_file_state == "static") mapped = "static";
    return mapped;
  }

  static constexpr long long state_stopped = 1;
  static constexpr long long state_starting = 2;
  static constexpr long long state_oneshot = 3;
  static constexpr long long state_running = 4;
  static constexpr long long state_static = 5;
  static constexpr long long state_unknown = 0;

  static long long state_to_int(const std::string &mapped) {
    if (mapped == "stopped") return state_stopped;
    if (mapped == "starting") return state_starting;
    if (mapped == "oneshot") return state_oneshot;
    if (mapped == "running") return state_running;
    if (mapped == "static") return state_static;
    return state_unknown;
  }

  long long get_state_i() const { return state_to_int(state); }

  // Parse a state string used in a threshold expression into its numeric value.
  static long long parse_state(const std::string &s) {
    if (s == "running" || s == "active" || s == "started") return state_running;
    if (s == "stopped" || s == "inactive" || s == "dead" || s == "failed") return state_stopped;
    if (s == "starting" || s == "activating") return state_starting;
    if (s == "oneshot" || s == "exited") return state_oneshot;
    if (s == "static") return state_static;
    return state_unknown;
  }

  // Active-state helpers (kept for backward compatibility of state_is_ok /
  // state_is_perfect and the started/stopped booleans).
  bool is_running() const { return state == "running"; }
  bool is_started() const { return active == "active"; }
  bool is_stopped() const { return active == "inactive" || active == "failed"; }
  bool is_failed() const { return active == "failed"; }

  long long get_started() const { return is_started() ? 1 : 0; }
  long long get_stopped() const { return is_stopped() ? 1 : 0; }

  // Start type checks
  bool is_enabled() const { return start_type == "enabled" || start_type == "enabled-runtime"; }
  bool is_disabled() const { return start_type == "disabled"; }
  bool is_static() const { return start_type == "static"; }
  bool is_masked() const { return start_type == "masked" || start_type == "masked-runtime"; }

  static constexpr long long start_type_enabled = 1;
  static constexpr long long start_type_disabled = 0;
  static constexpr long long start_type_static = 2;
  static constexpr long long start_type_masked = -1;
  static constexpr long long start_type_unknown = -10;

  long long get_start_type_i() const {
    if (is_enabled()) return start_type_enabled;
    if (is_disabled()) return start_type_disabled;
    if (is_static()) return start_type_static;
    if (is_masked()) return start_type_masked;
    return start_type_unknown;
  }

  static long long parse_start_type(const std::string &s) {
    if (s == "enabled" || s == "auto") return start_type_enabled;
    if (s == "disabled" || s == "manual") return start_type_disabled;
    if (s == "static") return start_type_static;
    if (s == "masked") return start_type_masked;
    return start_type_unknown;
  }

  // Check if state is as expected based on start_type
  bool state_is_ok() const {
    if (is_masked()) return is_stopped();
    if (is_disabled()) return true;  // Can be either running or stopped
    if (is_enabled()) return is_started();
    if (is_static()) return true;  // Static services are OK in any state
    return true;
  }

  bool state_is_perfect() const {
    if (is_masked()) return is_stopped();
    if (is_disabled()) return is_stopped();
    if (is_enabled()) return is_running();
    if (is_static()) return true;
    return true;
  }
};

// Per-process metrics parsed from /proc, used to populate a running service's
// rss/vms/cpu/created/age. Kept separate for unit testing.
struct proc_metrics {
  long long rss;
  long long vms;
  long long created;
  long long age;
  double cpu;
  bool valid;
  proc_metrics() : rss(0), vms(0), created(0), age(0), cpu(0), valid(false) {}
};

// Parse VmRSS/VmSize (kB) from the contents of /proc/<pid>/status into bytes.
bool parse_status_mem(const std::string &status_content, long long &rss_bytes, long long &vms_bytes);

// Parse utime/stime/starttime (in clock ticks) from /proc/<pid>/stat contents.
bool parse_stat_times(const std::string &stat_content, unsigned long long &utime, unsigned long long &stime, unsigned long long &starttime);

// Compute lifetime-average CPU percent for a process. Returns 0 for a
// non-positive elapsed window.
double compute_cpu_pct(unsigned long long utime, unsigned long long stime, unsigned long long starttime, double uptime_secs, long long hz);

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// Parse `systemctl show` output (one or more property blocks separated by blank
// lines) into filter_obj rows (without process metrics).
std::vector<filter_obj> parse_systemctl_show(const std::string &output);

}  // namespace check_svc_filter

// Evaluate a pre-built list of services against the default (or request-
// supplied) filter/thresholds. Exposed for unit testing.
void check_service_evaluate(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                            const std::vector<check_svc_filter::filter_obj> &services);

void check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace checks

#endif  // NSCP_CHECK_SERVICE_H
