// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The pending-reboot reader on Linux: the real filesystem root and the
// running kernel from uname(2). Which files say a reboot is pending, and how
// kernels are compared, is platform-neutral and lives in
// check_pending_reboot.cpp.

#include <sys/utsname.h>

#include <cstring>
#include <ctime>

#include "check_pending_reboot.h"

pending_reboot_check::reboot_obj pending_reboot_check::gather_pending_reboot() {
  utsname uts;
  std::memset(&uts, 0, sizeof(uts));
  const std::string running = ::uname(&uts) == 0 ? std::string(uts.release) : std::string();
  return gather_pending_reboot_from("", running, static_cast<long long>(std::time(nullptr)));
}
