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

#include "check_nla.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace nla_filter {
using parsers::where::type_bool;
filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("network", &filter_obj::get_network, "Network name")
      .add_string_var("category", &filter_obj::get_category, "Network category: public, private or domain");
  registry_.add_int_var("connected", type_bool, &filter_obj::get_connected, "True if the network is currently connected");
}
}  // namespace nla_filter

namespace check_nla_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<nla_filter::filter> filter_helper(request, response, data);

  nla_filter::filter filter;
  // No default alert: the operator asserts the expected posture, e.g.
  // "crit=connected = 1 and category = 'public'".
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${network}=${category}", "${network}", "No networks found", "${status}: all networks ok");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<nla_filter::filter_obj_ptr> networks;
  std::string error;
  nla_source::gather(networks, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const nla_filter::filter_obj_ptr &n : networks) filter.match(n);
  filter_helper.post_process(filter);
}

}  // namespace check_nla_command
