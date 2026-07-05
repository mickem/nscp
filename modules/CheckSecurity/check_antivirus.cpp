// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_antivirus.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace antivirus_filter {
using parsers::where::type_bool;
using parsers::where::type_int;
filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Antivirus product display name");
  registry_.add_int_var("enabled", type_bool, &filter_obj::get_enabled, "True if real-time protection is enabled")
      .add_int_var("up_to_date", type_bool, &filter_obj::get_up_to_date, "True if virus definitions are current")
      .add_int_var("product_state", type_int, &filter_obj::get_product_state, "Raw Security Center productState bitfield");
}
}  // namespace antivirus_filter

namespace check_antivirus_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<antivirus_filter::filter> filter_helper(request, response, data);

  antivirus_filter::filter filter;
  // Default: critical if any product is disabled or has stale definitions.
  filter_helper.add_options("", "enabled = 0 or up_to_date = 0", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${name} (enabled=${enabled} up_to_date=${up_to_date})", "${name}", "No antivirus product registered",
                           "${status}: ${count} antivirus product(s) healthy");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<antivirus_filter::filter_obj_ptr> products;
  std::string error;
  antivirus_source::gather(products, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const antivirus_filter::filter_obj_ptr &p : products) filter.match(p);
  filter_helper.post_process(filter);
}

}  // namespace check_antivirus_command
