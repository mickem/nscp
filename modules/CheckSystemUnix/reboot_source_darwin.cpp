// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Darwin reader for check_pending_reboot. There is none yet: macOS has no
// reboot marker, and the signal would be softwareupdate's restart-required
// flag on a staged update. The check reports this error (UNKNOWN) rather
// than a "No reboot pending" it cannot vouch for.

#include <stdexcept>

#include "check_pending_reboot.h"

pending_reboot_check::reboot_obj pending_reboot_check::gather_pending_reboot() {
  throw std::runtime_error("check_pending_reboot is not supported on macOS yet");
}
