// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// macOS stubs for the posture checks that have Windows and Linux sources but no
// Darwin reader yet. macOS keeps its accounts and groups in Directory Services
// (not /etc/passwd and /etc/group) and has no UEFI variable interface, so the
// Linux readers would answer wrongly here; report "not supported" instead.

#include "check_group_members.hpp"
#include "check_local_accounts.hpp"
#include "check_secureboot.hpp"

namespace secureboot_source {
void gather(std::vector<secureboot_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_secureboot is not supported on this platform (Windows and Linux UEFI only)";
}
}  // namespace secureboot_source

namespace local_accounts_source {
void gather(std::vector<local_accounts_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_local_accounts is not supported on this platform (Windows and Linux local accounts only)";
}
}  // namespace local_accounts_source

namespace group_members_source {
std::string default_group() { return "admin"; }
void gather(const std::string & /*group*/, std::vector<group_members_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_group_members is not supported on this platform (Windows and Linux local groups only)";
}
}  // namespace group_members_source
