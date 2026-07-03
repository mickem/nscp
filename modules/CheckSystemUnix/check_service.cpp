#include "check_service.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <vector>

namespace po = boost::program_options;

using namespace parsers::where;

namespace checks {

namespace check_svc_filter {

constexpr auto DEFAULT_WARNING = "";
constexpr auto DEFAULT_CRIT = "( state not in ('running', 'oneshot', 'static') or active = 'failed' ) and preset != 'disabled'";
constexpr auto DEFAULT_FILTER = "active != 'inactive'";

// Case-insensitive string comparison
struct CaseBlindCompare {
  bool operator()(const std::string &a, const std::string &b) const { return boost::ilexicographical_compare(a, b); }
};

// Allowlist for systemd unit names. Rejects anything that could be parsed as a
// flag or contain shell/path metacharacters. Empty on bad input.
bool is_safe_unit_name(const std::string &name) {
  if (name.empty() || name.size() > 256) return false;
  if (name[0] == '-') return false;
  for (char c : name) {
    const bool ok =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == '@' || c == ':' || c == '\\';
    if (!ok) return false;
  }
  return true;
}

// Execute a program directly (no shell) and capture stdout. argv[0] is the
// program; remaining elements are arguments passed verbatim to execvp.
std::string exec_command(const std::vector<std::string> &argv) {
  if (argv.empty()) return "";

  int pipefd[2];
  if (pipe(pipefd) == -1) return "";

  const pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return "";
  }

  if (pid == 0) {
    close(pipefd[0]);
    if (dup2(pipefd[1], STDOUT_FILENO) == -1) _exit(127);
    const int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    close(pipefd[1]);

    std::vector<char *> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto &a : argv) cargv.push_back(const_cast<char *>(a.c_str()));
    cargv.push_back(nullptr);

    execvp(cargv[0], cargv.data());
    _exit(127);
  }

  close(pipefd[1]);
  std::array<char, 4096> buffer{};
  std::string result;
  ssize_t n;
  while ((n = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
    result.append(buffer.data(), static_cast<size_t>(n));
  }
  close(pipefd[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  return result;
}

// ---- /proc process-metric parsing (unit-tested) --------------------------

bool parse_status_mem(const std::string &status_content, long long &rss_bytes, long long &vms_bytes) {
  std::istringstream iss(status_content);
  std::string line;
  bool have_rss = false, have_vms = false;
  while (std::getline(iss, line)) {
    if (line.compare(0, 6, "VmRSS:") == 0) {
      std::istringstream ls(line.substr(6));
      long long kb = 0;
      if (ls >> kb) {
        rss_bytes = kb * 1024;
        have_rss = true;
      }
    } else if (line.compare(0, 7, "VmSize:") == 0) {
      std::istringstream ls(line.substr(7));
      long long kb = 0;
      if (ls >> kb) {
        vms_bytes = kb * 1024;
        have_vms = true;
      }
    }
  }
  return have_rss || have_vms;
}

bool parse_stat_times(const std::string &stat_content, unsigned long long &utime, unsigned long long &stime, unsigned long long &starttime) {
  // /proc/<pid>/stat: "pid (comm) state ppid ... utime stime ... starttime ...".
  // comm may contain spaces and parentheses, so split on the LAST ')'.
  const std::string::size_type close = stat_content.find_last_of(')');
  if (close == std::string::npos) return false;
  std::istringstream rest(stat_content.substr(close + 1));
  std::vector<std::string> tok;
  std::string t;
  while (rest >> t) tok.push_back(t);
  // After "pid (comm)", the remaining fields start at field 3 (state). So the
  // token index for field N is (N - 3): utime=14 -> 11, stime=15 -> 12,
  // starttime=22 -> 19.
  if (tok.size() < 20) return false;
  try {
    utime = std::stoull(tok[11]);
    stime = std::stoull(tok[12]);
    starttime = std::stoull(tok[19]);
  } catch (...) {
    return false;
  }
  return true;
}

double compute_cpu_pct(unsigned long long utime, unsigned long long stime, unsigned long long starttime, double uptime_secs, long long hz) {
  if (hz <= 0) return 0.0;
  const double proc_secs = static_cast<double>(utime + stime) / static_cast<double>(hz);
  const double start_secs = static_cast<double>(starttime) / static_cast<double>(hz);
  const double elapsed = uptime_secs - start_secs;
  if (elapsed <= 0.0) return 0.0;
  return proc_secs / elapsed * 100.0;
}

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

// TasksCurrent is unset (a huge sentinel) for units without a cgroup task
// count; clamp such values to 0.
long long parse_tasks(const std::string &value) {
  try {
    const unsigned long long v = std::stoull(value);
    if (v > 1000000ULL) return 0;  // [not set] sentinel is 2^64-1
    return static_cast<long long>(v);
  } catch (...) {
    return 0;
  }
}
}  // namespace

std::vector<filter_obj> parse_systemctl_show(const std::string &output) {
  std::vector<filter_obj> result;
  std::istringstream iss(output);
  std::string line;
  std::map<std::string, std::string> block;

  auto flush = [&]() {
    if (block.empty()) return;
    filter_obj info;
    std::string id = block.count("Id") ? block["Id"] : "";
    if (boost::ends_with(id, ".service")) id = id.substr(0, id.size() - 8);
    info.name = id;
    info.desc = block["Description"];
    info.load_state = block["LoadState"];
    info.active = block["ActiveState"];
    info.sub_state = block["SubState"];
    info.start_type = block["UnitFileState"];
    info.preset = block["UnitFilePreset"];
    if (block.count("MainPID")) {
      try {
        info.pid = std::stoi(block["MainPID"]);
      } catch (...) {
        info.pid = 0;
      }
    }
    if (block.count("TasksCurrent")) info.tasks = parse_tasks(block["TasksCurrent"]);
    info.state = filter_obj::map_state(info.active, info.sub_state, info.start_type);
    result.push_back(info);
    block.clear();
  };

  while (std::getline(iss, line)) {
    if (line.empty() || line.find_first_not_of(" \t\r") == std::string::npos) {
      flush();
      continue;
    }
    const std::string::size_type pos = line.find('=');
    if (pos == std::string::npos) continue;
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    boost::trim_right(value);
    block[key] = value;
  }
  flush();
  return result;
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

// Parse state helper functions for the filter
node_type parse_state(std::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  return factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
}

node_type parse_start_type(std::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  return factory::create_int(filter_obj::parse_start_type(subject->get_string_value(context)));
}

node_type state_is_ok(const value_type, evaluation_context context, const node_type subject) {
  auto *native = reinterpret_cast<native_context *>(context.get());
  if (!native->has_object()) {
    context->error("No object available");
    return factory::create_false();
  }
  return native->get_object()->state_is_ok() ? factory::create_true() : factory::create_false();
}

node_type state_is_perfect(const value_type, evaluation_context context, const node_type subject) {
  auto *native = reinterpret_cast<native_context *>(context.get());
  if (!native->has_object()) {
    context->error("No object available");
    return factory::create_false();
  }
  return native->get_object()->state_is_perfect() ? factory::create_true() : factory::create_false();
}

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_state = type_custom_int_1;
  static constexpr value_type type_custom_start_type = type_custom_int_2;

  registry_.add_string_var("name", &filter_obj::get_name, "Service name")
      .add_string_var("service", &filter_obj::get_name, "Alias for name")
      .add_string_var("desc", &filter_obj::get_desc, "Service description")
      .add_string_var("active", &filter_obj::get_active, "Raw systemd ActiveState (active, inactive, failed)")
      .add_string_var("sub_state", &filter_obj::get_sub_state, "Raw systemd SubState (running, dead, exited, ...)")
      .add_string_var("preset", &filter_obj::get_preset, "Vendor preset (enabled, disabled)");

  registry_.add_int_var("pid", &filter_obj::get_pid, "Main process id")
      .add_int_var("state", type_custom_state, &filter_obj::get_state_i,
                   "The mapped service state (stopped, starting, oneshot, running, static, unknown)")
      .add_int_perf("", "")
      .add_int_var("start_type", type_custom_start_type, &filter_obj::get_start_type_i, "The configured start type (enabled, disabled, static, masked)")
      .add_int_var("started", type_bool, &filter_obj::get_started, "Service is started/active")
      .add_int_var("stopped", type_bool, &filter_obj::get_stopped, "Service is stopped/inactive")
      .add_int_var("rss", &filter_obj::get_rss, "Resident memory of the main process in bytes")
      .add_int_perf("B")
      .add_int_var("vms", &filter_obj::get_vms, "Virtual memory of the main process in bytes")
      .add_int_perf("B")
      .add_int_var("tasks", &filter_obj::get_tasks, "Number of tasks (cgroup) for this service")
      .add_int_perf("")
      .add_int_var("created", type_date, &filter_obj::get_created, "Unix timestamp when the main process started")
      .add_int_var("age", &filter_obj::get_age, "Seconds since the main process started");

  registry_.add_float("cpu", &filter_obj::get_cpu, "CPU usage of the main process in percent (lifetime average)").add_float_perf("%");

  // clang-format off
  registry_.add_custom_fun("state_is_perfect", type_bool, &state_is_perfect, "Check if the state is perfect (enabled services running, disabled services stopped)")
    .add_custom_fun("state_is_ok", type_bool, &state_is_ok, "Check if the state is ok (enabled services running or starting, disabled services can be any state)")
    ;
  // clang-format on

  registry_.add_human_string("state", &filter_obj::get_state_s, "The mapped service state")
      .add_human_string("start_type", &filter_obj::get_start_type_s, "The configured start type");

  registry_.add_converter(type_custom_state, &parse_state).add_converter(type_custom_start_type, &parse_start_type);
}

// Get one service's info via `systemctl show`, then attach process metrics.
filter_obj get_service_info(const std::string &service_name, const sys_timing &timing) {
  filter_obj info;
  info.name = service_name;

  if (!is_safe_unit_name(service_name)) {
    return info;
  }

  const std::vector<filter_obj> parsed = parse_systemctl_show(exec_command({"systemctl", "show", "--no-pager", "--", service_name}));
  if (!parsed.empty()) info = parsed.front();

  // Fall back to is-enabled when the show output lacked UnitFileState.
  if (info.start_type.empty()) {
    std::string enabled_output = exec_command({"systemctl", "is-enabled", "--", service_name});
    boost::trim(enabled_output);
    if (!enabled_output.empty()) info.start_type = enabled_output;
  }

  fill_metrics(info, timing);
  return info;
}

// List all service unit names via systemctl list-units.
std::vector<std::string> list_service_units() {
  std::vector<std::string> names;
  const std::string output = exec_command({"systemctl", "list-units", "--type=service", "--all", "--no-legend", "--plain", "--no-pager"});
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

// Enumerate all services: one bulk `systemctl show` for every unit, then
// process metrics from /proc (no extra forks per service).
std::vector<filter_obj> enumerate_services(const std::string & /*type*/, const std::string &state_filter, const sys_timing &timing) {
  std::vector<filter_obj> result;
  const std::vector<std::string> names = list_service_units();
  if (names.empty()) return result;

  std::vector<std::string> argv = {"systemctl", "show", "--no-pager", "--"};
  argv.insert(argv.end(), names.begin(), names.end());
  std::vector<filter_obj> parsed = parse_systemctl_show(exec_command(argv));

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

// Set up the filter with the shared defaults and match a pre-built list of
// services. Used both by the live check (after enumeration) and by unit tests
// (with synthetic services), so the default warn/crit/filter are exercised
// exactly as shipped.
void check_service_evaluate(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                            const std::vector<check_svc_filter::filter_obj> &services) {
  typedef check_svc_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> services_arg, excludes;
  std::string state;

  filter_type filter;
  filter_helper.add_options(check_svc_filter::DEFAULT_WARNING, check_svc_filter::DEFAULT_CRIT, check_svc_filter::DEFAULT_FILTER, filter.get_filter_syntax(),
                            "unknown");
  filter_helper.add_syntax("${status}: ${crit_list}", "${name}=${state}", "${name}", "%(status): No services found",
                           "%(status): All %(count) service(s) are ok.");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("service", po::value<std::vector<std::string>>(&services_arg), "The service to check")
    ("exclude", po::value<std::vector<std::string>>(&excludes), "A list of services to ignore")
    ("state", po::value<std::string>(&state)->default_value("all"), "The state of services to enumerate")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  for (const check_svc_filter::filter_obj &info : services) {
    if (std::find(excludes.begin(), excludes.end(), info.name) != excludes.end()) continue;
    std::shared_ptr<check_svc_filter::filter_obj> record(new check_svc_filter::filter_obj(info));
    filter.match(record);
    if (filter.has_errors()) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed (see log for details)");
    }
  }

  filter_helper.post_process(filter);
}

void check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  const check_svc_filter::sys_timing timing = check_svc_filter::read_sys_timing();

  // `fetch-only` short-circuits the filter machinery and emits one line per
  // service in `<<<services>>>` format: name state/start_type display.
  for (int i = 0; i < request.arguments_size(); i++) {
    const std::string &a = request.arguments(i);
    if (a == "fetch-only" || a == "--fetch-only") {
      std::string body;
      const std::vector<check_svc_filter::filter_obj> svcs = check_svc_filter::enumerate_services("service", "all", timing);
      for (const check_svc_filter::filter_obj &s : svcs) {
        if (!body.empty()) body += "\n";
        body += s.name + " " + s.state + "/" + s.start_type + " " + (s.desc.empty() ? s.name : s.desc);
      }
      nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_service", NSCAPI::query_return_codes::returnOK, body, "");
      return;
    }
  }

  typedef check_svc_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> services, excludes;
  std::string state;

  filter_type filter;
  filter_helper.add_options(check_svc_filter::DEFAULT_WARNING, check_svc_filter::DEFAULT_CRIT, check_svc_filter::DEFAULT_FILTER, filter.get_filter_syntax(),
                            "unknown");
  filter_helper.add_syntax("${status}: ${crit_list}", "${name}=${state}", "${name}", "%(status): No services found",
                           "%(status): All %(count) service(s) are ok.");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("service", po::value<std::vector<std::string>>(&services), "The service to check, set this to * to check all services")
    ("exclude", po::value<std::vector<std::string>>(&excludes), "A list of services to ignore (mainly useful in combination with service=*)")
    ("state", po::value<std::string>(&state)->default_value("all"), "The state of services to enumerate: active, inactive, failed, or all")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (services.empty()) {
    services.emplace_back("*");
  }

  if (!filter_helper.build_filter(filter)) return;

  for (const std::string &service : services) {
    if (service == "*") {
      // Enumerate all services
      std::vector<check_svc_filter::filter_obj> service_list = check_svc_filter::enumerate_services("service", state, timing);
      for (const check_svc_filter::filter_obj &info : service_list) {
        // Check excludes
        if (std::find(excludes.begin(), excludes.end(), info.name) != excludes.end()) continue;

        std::shared_ptr<check_svc_filter::filter_obj> record(new check_svc_filter::filter_obj(info));
        filter.match(record);
        if (filter.has_errors()) {
          return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed (see log for details)");
        }
      }
    } else {
      // Get specific service
      std::string service_name = service;
      // Add .service suffix if not present
      if (!boost::ends_with(service_name, ".service")) {
        service_name += ".service";
      }

      check_svc_filter::filter_obj info = check_svc_filter::get_service_info(service_name, timing);

      // Remove .service suffix for display
      if (boost::ends_with(info.name, ".service")) {
        info.name = info.name.substr(0, info.name.length() - 8);
      }

      std::shared_ptr<check_svc_filter::filter_obj> record(new check_svc_filter::filter_obj(info));
      filter.match(record);
      if (filter.has_errors()) {
        return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed (see log for details)");
      }
    }
  }

  filter_helper.post_process(filter);
}

}  // namespace checks
