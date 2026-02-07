#ifndef NSCP_CHECK_SERVICE_H
#define NSCP_CHECK_SERVICE_H
#include <boost/shared_ptr.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace checks {

namespace check_svc_filter {

struct filter_obj {
  std::string name;
  std::string desc;
  std::string state;       // running, stopped, failed, etc.
  std::string sub_state;   // dead, running, exited, etc.
  std::string load_state;  // loaded, not-found, masked, etc.
  std::string start_type;  // enabled, disabled, static, masked, etc.
  int pid;

  filter_obj() : pid(0) {}

  filter_obj(const filter_obj &other) = default;

  std::string get_name() const { return name; }
  std::string get_desc() const { return desc; }
  std::string get_state_s() const { return state; }
  std::string get_sub_state() const { return sub_state; }
  std::string get_load_state() const { return load_state; }
  std::string get_start_type_s() const { return start_type; }
  long long get_pid() const { return pid; }

  std::string show() const { return name + ":" + state; }

  // State checks
  bool is_running() const { return state == "active" && sub_state == "running"; }
  bool is_started() const { return state == "active"; }
  bool is_stopped() const { return state == "inactive" || state == "failed"; }
  bool is_failed() const { return state == "failed"; }

  long long get_started() const { return is_started() ? 1 : 0; }
  long long get_stopped() const { return is_stopped() ? 1 : 0; }

  // State integer mapping
  static constexpr long long state_running = 1;
  static constexpr long long state_stopped = 0;
  static constexpr long long state_failed = -1;
  static constexpr long long state_unknown = -10;

  long long get_state_i() const {
    if (state == "active") return state_running;
    if (state == "inactive") return state_stopped;
    if (state == "failed") return state_failed;
    return state_unknown;
  }

  static long long parse_state(const std::string &s) {
    if (s == "running" || s == "active" || s == "started") return state_running;
    if (s == "stopped" || s == "inactive" || s == "dead") return state_stopped;
    if (s == "failed") return state_failed;
    return state_unknown;
  }

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

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_svc_filter

void check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace checks

#endif  // NSCP_CHECK_SERVICE_H
