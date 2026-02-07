#include "check_cpu.h"

#include <boost/program_options.hpp>
#include <map>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/helpers.hpp>
#include <str/format.hpp>

#include "realtime_thread.hpp"

namespace po = boost::program_options;

using namespace parsers::where;

namespace checks {
namespace check_cpu_filter {

node_type calculate_load(boost::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  helpers::read_arg_type value = helpers::read_arguments(context, subject, "%");
  double number = value.get<1>();
  std::string unit = value.get<2>();

  if (unit != "%") context->error("Invalid unit: " + unit);
  return factory::create_int(llround(number));
}

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_pct = type_custom_int_1;

  registry_.add_string("time", &filter_obj::get_time, "The time frame to check")
      .add_string("core", &filter_obj::get_core_s, &filter_obj::get_core_i, "The core to check (total or core ##)")
      .add_string("core_id", &filter_obj::get_core_id, &filter_obj::get_core_i, "The core to check (total or core_##)");

  registry_.add_int_x("load", type_custom_pct, &filter_obj::get_total, "The current load for a given core (deprecated, use total)")
      .add_int_perf("%")
      .add_int_x("total", type_custom_pct, &filter_obj::get_total, "The current load used by user and system")
      .add_int_perf("%")
      .add_int_x("user", type_custom_pct, &filter_obj::get_user, "The current load used by user applications")
      .add_int_perf("%")
      .add_int_x("idle", &filter_obj::get_idle, "The current idle load for a given core")
      .add_int_x("system", &filter_obj::get_kernel, "The current load used by the system (kernel)")
      .add_int_x("kernel", &filter_obj::get_kernel, "deprecated (use system instead)");

  registry_.add_converter()(type_custom_pct, &calculate_load);
}
}  // namespace check_cpu_filter

void check_cpu(boost::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
               PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_cpu_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> times;
  bool show_all_cores = false;

  filter_type filter;
  filter_helper.add_options("load > 80", "load > 90", "core = 'total'", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${problem_list}", "${time}: ${load}%", "${core} ${time}", "", "%(status): CPU load is ok.");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("time", po::value<std::vector<std::string>>(&times), "The time to check")
    ("cores", boost::program_options::bool_switch(&show_all_cores),
    "This will remove the filter to include the cores, if you use filter don't use this as well.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (show_all_cores && filter_helper.data.filter_string.size() == 1) {
    filter_helper.data.filter_string.clear();
  }

  if (times.empty()) {
    times.emplace_back("5s");
    times.emplace_back("1m");
    times.emplace_back("5m");
  }

  if (!filter_helper.build_filter(filter)) return;

  // Check if collector is available and has data
  if (!collector) {
    return nscapi::protobuf::functions::set_response_bad(*response, "CPU collector not initialized");
  }

  if (!collector->has_cpu_data()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No CPU data available yet (collector still initializing)");
  }

  for (const std::string &time : times) {
    const long seconds = str::format::decode_time<long>(time, 1);
    auto cpu_data = collector->get_cpu_load(seconds);

    if (cpu_data.empty()) {
      NSC_LOG_ERROR("No CPU data returned for time period: " + time);
      continue;
    }

    for (const auto &entry : cpu_data) {
      const load_entry &load = entry.second;
      const boost::shared_ptr<check_cpu_filter::filter_obj> record(new check_cpu_filter::filter_obj(time, entry.first, load.user, load.kernel, load.idle));
      filter.match(record);
    }
  }

  filter_helper.post_process(filter);
}

}  // namespace checks
