// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// macOS stubs for the posture checks that have Windows and Linux sources but no
// Darwin reader yet. macOS has no UEFI variable interface, so the Linux reader
// would answer wrongly here; report "not supported" instead.

#include "check_secureboot.hpp"

namespace secureboot_source {
void gather(std::vector<secureboot_filter::filter_obj_ptr> & /*out*/, std::string &error) {
  error = "check_secureboot is not supported on this platform (Windows and Linux UEFI only)";
}
}  // namespace secureboot_source
