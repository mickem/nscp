#ifndef NSCP_CHECK_PROCESS_H
#define NSCP_CHECK_PROCESS_H
#include <boost/shared_ptr.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>

namespace check_proc {

namespace check_proc_filter {

struct filter_obj {
  std::string filename;
  std::string exe;
  std::string command_line;
  int pid;
  bool started;
  std::string error;

  // Memory counters
  unsigned long long virtual_size;
  unsigned long long working_set;
  unsigned long long page_faults;

  // Time counters (in seconds)
  unsigned long long user_time;
  unsigned long long kernel_time;

  filter_obj() : pid(0), started(false), virtual_size(0), working_set(0), page_faults(0), user_time(0), kernel_time(0) {}

  filter_obj(const std::string &exe_name)
      : exe(exe_name), pid(0), started(false), virtual_size(0), working_set(0), page_faults(0), user_time(0), kernel_time(0) {}

  filter_obj(const filter_obj &other) = default;

  std::string show() const { return filename + ", " + command_line + ", pid: " + std::to_string(pid) + ", state: " + get_state_s(); }

  std::string get_filename() const { return filename; }
  std::string get_exe() const { return exe; }
  std::string get_command_line() const { return command_line; }
  std::string get_error() const { return error; }
  long long get_pid() const { return pid; }

  bool get_started() const { return started; }
  bool get_stopped() const { return !started; }
  bool get_has_error() const { return !error.empty(); }

  std::string get_state_s() const {
    if (!error.empty()) return "error";
    if (started) return "started";
    return "stopped";
  }

  static constexpr long long state_started = 1;
  static constexpr long long state_stopped = 0;
  static constexpr long long state_unknown = -10;

  long long get_state_i() const {
    if (started) return state_started;
    return state_stopped;
  }

  static long long parse_state(const std::string &s) {
    if (s == "started") return state_started;
    if (s == "stopped") return state_stopped;
    return state_unknown;
  }

  // Memory getters
  long long get_virtual_size() const { return virtual_size; }
  long long get_working_set() const { return working_set; }
  long long get_page_faults() const { return page_faults; }

  std::string get_virtual_size_human() const { return str::format::format_byte_units(virtual_size); }
  std::string get_working_set_human() const { return str::format::format_byte_units(working_set); }

  // Time getters
  long long get_user_time() const { return user_time; }
  long long get_kernel_time() const { return kernel_time; }
  long long get_total_time() const { return user_time + kernel_time; }

  // For aggregation
  filter_obj &operator+=(const filter_obj &other) {
    virtual_size += other.virtual_size;
    working_set += other.working_set;
    page_faults += other.page_faults;
    user_time += other.user_time;
    kernel_time += other.kernel_time;
    return *this;
  }
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_proc_filter

void check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_proc

#endif  // NSCP_CHECK_PROCESS_H
