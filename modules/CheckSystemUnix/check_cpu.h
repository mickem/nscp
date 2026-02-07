#ifndef NSCP_CHECK_CPU_H
#define NSCP_CHECK_CPU_H
#include <boost/algorithm/string/replace.hpp>
#include <boost/shared_ptr.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <utility>

class pdh_thread;

namespace checks {

namespace check_cpu_filter {

struct filter_obj {
  std::string time;
  std::string core;
  double user;
  double kernel;
  double idle;

  filter_obj(std::string time, std::string core, double user, double kernel, double idle)
      : time(std::move(time)), core(std::move(core)), user(user), kernel(kernel), idle(idle) {}

  std::string show() const { return "core: " + core + " time: " + time + " load: " + std::to_string(static_cast<long long>(get_total())) + "%"; }
  long long get_total() const { return static_cast<long long>(user + kernel); }
  long long get_idle() const { return static_cast<long long>(idle); }
  long long get_kernel() const { return static_cast<long long>(kernel); }
  long long get_user() const { return static_cast<long long>(user); }
  std::string get_time() const { return time; }
  std::string get_core_s() const { return core; }
  std::string get_core_id() const { return boost::replace_all_copy(core, " ", "_"); }
  long long get_core_i() const {
    if (core == "total") return -1;
    try {
      // Extract core number from "core X"
      if (core.substr(0, 5) == "core ") {
        return std::stoll(core.substr(5));
      }
    } catch (...) {
    }
    return 0;
  }
};
typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace check_cpu_filter

void check_cpu(boost::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
               PB::Commands::QueryResponseMessage::Response *response);

}  // namespace checks

#endif  // NSCP_CHECK_CPU_H
