#include "check_pagefile.h"

#include <boost/program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/helpers.hpp>

#include "realtime_thread.hpp"

namespace po = boost::program_options;

using namespace parsers::where;

namespace check_page {

namespace check_page_filter {

node_type calculate_free(boost::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  helpers::read_arg_type value = helpers::read_arguments(context, subject, "%");
  long long number = value.get<1>();
  std::string unit = value.get<2>();

  if (unit == "%") {
    number = (object->get_total() * number) / 100;
  } else {
    number = str::format::decode_byte_units(number, unit);
  }
  return factory::create_int(number);
}

long long get_zero() { return 0; }

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_used = type_custom_int_1;
  static constexpr value_type type_custom_free = type_custom_int_2;

  registry_.add_string("name", &filter_obj::get_name, "The name of the page file (swap)");
  // clang-format off
  registry_
      .add_int()
        ("size", [] (auto obj, auto context) { return obj->get_total(); }, "Total size of pagefile/swap")
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

}  // namespace check_page_filter

void check_pagefile(boost::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                    PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_page_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("used > 60%", "used > 80%", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${name} ${used} (${size})", "${name}", "", "");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  // Check if collector is available and has data
  if (!collector) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Pagefile collector not initialized");
  }

  if (!collector->has_memory_data()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No pagefile/swap data available yet (collector still initializing)");
  }

  // Get memory data from collector (use 1 second average for current snapshot)
  memory_info mem_data = collector->get_memory(1);

  // On Linux, pagefile = swap
  const unsigned long long swap_total = mem_data.swap.total;
  const unsigned long long swap_used = mem_data.swap.get_used();

  // Create filter object for swap (named "total" to match Windows behavior)
  const boost::shared_ptr<check_page_filter::filter_obj> record(new check_page_filter::filter_obj("total", swap_total, swap_used));
  filter.match(record);

  filter_helper.post_process(filter);
}

}  // namespace check_page
