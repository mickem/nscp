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

// Unix stubs for the Windows-only posture checks. Each models a Windows-specific
// facility (network profiles, Security Center, BitLocker, UEFI Secure Boot) with
// no Linux equivalent, so they report "not supported" rather than pretend.

#include "check_antivirus.hpp"
#include "check_bitlocker.hpp"
#include "check_nla.hpp"
#include "check_secureboot.hpp"

namespace nla_source {
void gather(std::vector<nla_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_nla is not supported on this platform (Windows Network Location Awareness only)";
}
}  // namespace nla_source

namespace antivirus_source {
void gather(std::vector<antivirus_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_antivirus is not supported on this platform (Windows Security Center only)";
}
}  // namespace antivirus_source

namespace bitlocker_source {
void gather(std::vector<bitlocker_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_bitlocker is not supported on this platform (Windows BitLocker only)";
}
}  // namespace bitlocker_source

namespace secureboot_source {
void gather(std::vector<secureboot_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_secureboot is not supported on this platform (Windows/UEFI only)";
}
}  // namespace secureboot_source
