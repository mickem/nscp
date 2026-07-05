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

#include "check_bitlocker.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace bitlocker_filter {
using parsers::where::type_bool;
using parsers::where::type_int;
filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("drive", &filter_obj::get_drive, "Drive letter of the volume");
  registry_.add_int_var("protected", type_bool, &filter_obj::get_protected, "True if BitLocker protection is on")
      .add_int_var("protection_status", type_int, &filter_obj::get_protection_status, "Raw protection status (0 off, 1 on, 2 unknown)")
      .add_int_var("conversion_status", type_int, &filter_obj::get_conversion_status, "Raw conversion status (0 decrypted, 1 encrypted, ...)");
}
}  // namespace bitlocker_filter

namespace check_bitlocker_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<bitlocker_filter::filter> filter_helper(request, response, data);

  bitlocker_filter::filter filter;
  // Default: critical if any volume is not protected.
  filter_helper.add_options("", "protected = 0", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${drive} protected=${protected}", "${drive}", "No encryptable volumes found",
                           "${status}: all ${count} volume(s) protected");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<bitlocker_filter::filter_obj_ptr> volumes;
  std::string error;
  bitlocker_source::gather(volumes, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const bitlocker_filter::filter_obj_ptr &v : volumes) filter.match(v);
  filter_helper.post_process(filter);
}

}  // namespace check_bitlocker_command
