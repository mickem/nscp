// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux entry points of check_secureboot and check_local_accounts: the live
// paths handed to the fixture-testable readers in secureboot_source_linux.cpp
// and local_accounts_source_linux.cpp.

#include <ctime>

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
