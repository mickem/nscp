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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace bitlocker_filter {

// One encryptable volume and its BitLocker protection state.
struct filter_obj {
  filter_obj() : protection_status(0), conversion_status(0), is_protected(0) {}
  std::string get_drive() const { return drive; }
  long long get_protection_status() const { return protection_status; }
  long long get_conversion_status() const { return conversion_status; }
  long long get_protected() const { return is_protected; }
  std::string show() const { return drive; }

  std::string drive;             // drive letter (may be empty for non-lettered volumes)
  long long protection_status;   // 0 off, 1 on, 2 unknown
  long long conversion_status;   // 0 fully decrypted, 1 fully encrypted, 2 encrypting, ...
  long long is_protected;        // 1 if protection_status == 1
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace bitlocker_filter

namespace bitlocker_source {
// Windows only (WMI Win32_EncryptableVolume); the Unix stub sets `error`.
void gather(std::vector<bitlocker_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace bitlocker_source

namespace check_bitlocker_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
