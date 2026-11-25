// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux entry points of the posture checks that have a Linux source: the live
// paths handed to the fixture-testable readers (secureboot_source_linux.cpp).

#include "check_secureboot.hpp"

namespace secureboot_source {
void gather(std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error) { gather_from("/sys/firmware/efi", out, error); }
}  // namespace secureboot_source
