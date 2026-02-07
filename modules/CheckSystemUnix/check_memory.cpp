#include "check_memory.h"

#include <boost/assign.hpp>
#include <list>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/helpers.hpp>
#include <str/utils.hpp>

#include "realtime_thread.hpp"

namespace po = boost::program_options;

using namespace parsers::where;
namespace check_memory {

namespace check_mem_filter {

node_type calculate_free(boost::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  helpers::read_arg_type value = helpers::read_arguments(context, subject, "%");
  long long number = value.get<1>();
  std::string unit = value.get<2>();

  if (unit == "%") {
    number = object->get_total() * number / 100;
  } else {
    number = str::format::decode_byte_units(number, unit);
  }
  return factory::create_int(number);
}

long long get_zero() { return 0; }

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_used = type_custom_int_1;
  static constexpr value_type type_custom_free = type_custom_int_2;

  registry_.add_string("type", &filter_obj::get_type, "The type of memory to check");
  // clang-format off
  registry_
      .add_int()
        ("size", [] (auto obj, auto context) { return obj->get_total(); }, "Total size of memory")
        ("free", type_custom_free, [] (auto obj, auto context) { return obj->get_free(); }, "Free memory in bytes (g,m,k,b) or percentages %")
          .add_scaled_byte([] (auto obj, auto context) { return get_zero(); }, [] (auto obj, auto context) { return obj->get_total(); })
          .add_percentage([] (auto obj, auto context) { return obj->get_total(); }, "", " %")
        ("used", type_custom_used, [] (auto obj, auto context) { return obj->get_used(); }, "Used memory in bytes (g,m,k,b) or percentages %")
          .add_scaled_byte([] (auto obj, auto context) { return get_zero(); }, [] (auto obj, auto context) { return obj->get_total(); })
          .add_percentage([] (auto obj, auto context) { return obj->get_total(); }, "", " %")
      ;
  // clang-format on
  registry_.add_human_string("size", &filter_obj::get_total_human, "")
      .add_human_string("free", &filter_obj::get_free_human, "")
      .add_human_string("used", &filter_obj::get_used_human, "");

  registry_.add_converter()(type_custom_free, &calculate_free)(type_custom_used, &calculate_free);
}
}  // namespace check_mem_filter

void check_memory(boost::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                  PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_mem_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> types;

  filter_type filter;
  filter_helper.add_options("used > 80%", "used > 90%", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${type} = ${used}", "${type}", "", "");
  filter_helper.get_desc().add_options()("type", po::value<std::vector<std::string> >(&types),
                                         "The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)");

  if (!filter_helper.parse_options()) return;

  if (types.empty()) {
    types.emplace_back("physical");
    types.emplace_back("cached");
    types.emplace_back("swap");
  }

  if (!filter_helper.build_filter(filter)) return;

  // Check if collector is available and has data
  if (!collector) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Memory collector not initialized");
  }

  if (!collector->has_memory_data()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No memory data available yet (collector still initializing)");
  }

  const memory_info mem_data = collector->get_memory(1);

  // Build filter objects from collected data
  std::map<std::string, check_mem_filter::filter_obj> mem_map;
  mem_map.emplace("physical", check_mem_filter::filter_obj("physical", mem_data.physical.free, mem_data.physical.total));
  mem_map.emplace("cached", check_mem_filter::filter_obj("cached", mem_data.cached.free, mem_data.cached.total));
  mem_map.emplace("swap", check_mem_filter::filter_obj("swap", mem_data.swap.free, mem_data.swap.total));

  for (const std::string &type : types) {
    auto it = mem_map.find(type);
    if (it != mem_map.end()) {
      const boost::shared_ptr<check_mem_filter::filter_obj> record(new check_mem_filter::filter_obj(it->second));
      filter.match(record);
    } else {
      return nscapi::protobuf::functions::set_response_bad(*response, "Invalid type: " + type);
    }
  }

  filter_helper.post_process(filter);
}

}  // namespace check_memory
