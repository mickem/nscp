/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "check_firewall.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace po = boost::program_options;

namespace firewall_filter {

using parsers::where::type_bool;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("profile", &filter_obj::get_profile, "Firewall profile name (Domain, Private or Public)")
      .add_string_var("inbound", &filter_obj::get_inbound, "Default inbound action (allow/block)")
      .add_string_var("outbound", &filter_obj::get_outbound, "Default outbound action (allow/block)");
  registry_.add_int_var("enabled", type_bool, &filter_obj::get_enabled, "True if the profile's firewall is enabled").add_int_perf("");
}

}  // namespace firewall_filter

namespace check_firewall_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<firewall_filter::filter> filter_helper(request, response, data);

  firewall_filter::filter filter;
  // Default: critical if any profile's firewall is disabled.
  filter_helper.add_options("", "enabled = 0", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_list}", "${profile}=${enabled}", "${profile}", "No firewall profiles found",
                           "%(status): all %(count) firewall profile(s) enabled");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<firewall_filter::filter_obj_ptr> profiles;
  std::string error;
  firewall_source::gather(profiles, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const firewall_filter::filter_obj_ptr &p : profiles) filter.match(p);
  filter_helper.post_process(filter);
}

}  // namespace check_firewall_command
