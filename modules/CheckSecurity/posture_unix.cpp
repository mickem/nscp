// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unix stubs for the Windows-only posture checks. Each models a Windows-specific
// facility (network profiles, Security Center, BitLocker, UEFI Secure Boot,
// Software Licensing, NT security descriptors) with no Linux equivalent, so they
// report "not supported" rather than pretend.

#include "check_activation.hpp"
#include "check_antivirus.hpp"
#include "check_bitlocker.hpp"
#include "check_defender.hpp"
#include "check_file_security.hpp"
#include "check_group_members.hpp"
#include "check_local_accounts.hpp"
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

namespace local_accounts_source {
void gather(std::vector<local_accounts_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_local_accounts is not supported on this platform (Windows local accounts only)";
}
}  // namespace local_accounts_source

namespace group_members_source {
void gather(const std::string & /*group*/, std::vector<group_members_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_group_members is not supported on this platform (Windows local groups only)";
}
}  // namespace group_members_source

namespace activation_source {
void gather(bool /*all_products*/, bool /*with_genuine*/, std::vector<activation_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_activation is not supported on this platform (Windows Software Licensing only)";
}
}  // namespace activation_source

namespace file_security_source {
bool supported() { return false; }
std::string service_binary(const std::string & /*service*/, std::string &error) {
  error = "check_file_security is not supported on this platform (Windows security descriptors only)";
  return {};
}
void inspect(file_security_filter::filter_obj &obj, std::vector<file_security_filter::ace> & /*aces*/) {
  obj.error = "check_file_security is not supported on this platform (Windows security descriptors only)";
}
}  // namespace file_security_source
