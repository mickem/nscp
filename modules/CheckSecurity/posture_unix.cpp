// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unix stubs for the Windows-only posture checks. Each models a Windows-specific
// facility (network profiles, Security Center, BitLocker, UEFI Secure Boot) with
// no Linux equivalent, so they report "not supported" rather than pretend.

#include "check_antivirus.hpp"
#include "check_bitlocker.hpp"
#include "check_defender.hpp"
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

namespace defender_source {
void gather(std::vector<defender_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_defender is not supported on this platform (Windows Microsoft Defender only)";
}
}  // namespace defender_source
