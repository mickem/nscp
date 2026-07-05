// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_secureboot.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace secureboot_filter {
using parsers::where::type_bool;
filter_obj_handler::filter_obj_handler() {
  registry_.add_int_var("enabled", type_bool, &filter_obj::get_enabled, "True if UEFI Secure Boot is enabled")
      .add_int_var("supported", type_bool, &filter_obj::get_supported, "True if the platform reports a Secure Boot state (UEFI)");
}
}  // namespace secureboot_filter

namespace check_secureboot_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<secureboot_filter::filter> filter_helper(request, response, data);

  secureboot_filter::filter filter;
  // Default: critical if Secure Boot is not enabled.
  filter_helper.add_options("", "enabled = 0", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "secure boot enabled=${enabled} supported=${supported}", "secureboot", "No Secure Boot state",
                           "${status}: secure boot is enabled");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<secureboot_filter::filter_obj_ptr> state;
  std::string error;
  secureboot_source::gather(state, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const secureboot_filter::filter_obj_ptr &s : state) filter.match(s);
  filter_helper.post_process(filter);
}

}  // namespace check_secureboot_command
