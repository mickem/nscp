// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux entry points of check_secureboot, check_local_accounts and
// check_group_members: the live paths handed to the fixture-testable readers in
// secureboot_source_linux.cpp, local_accounts_source_linux.cpp and
// group_members_source_linux.cpp.

#include <ctime>

#include "check_group_members.hpp"
#include "check_local_accounts.hpp"
#include "check_secureboot.hpp"

namespace secureboot_source {
void gather(std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error) { gather_from("/sys/firmware/efi", out, error); }
}  // namespace secureboot_source

namespace local_accounts_source {
void gather(std::vector<local_accounts_filter::filter_obj_ptr> &out, std::string &error) {
  gather_from("/etc/passwd", "/etc/shadow", static_cast<long long>(std::time(nullptr)), out, error);
}
}  // namespace local_accounts_source

namespace group_members_source {
std::string default_group() {
  std::string content, error;
  if (!local_accounts_source::read_text_file("/etc/group", content, error)) return "";
  return first_existing_group(content, {"sudo", "wheel"});
}
void gather(const std::string &group, std::vector<group_members_filter::filter_obj_ptr> &out, std::string &error) {
  gather_from("/etc/group", "/etc/passwd", group, out, error);
}
}  // namespace group_members_source
