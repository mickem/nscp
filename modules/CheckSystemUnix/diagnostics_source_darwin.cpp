// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Darwin has no systemd, so the services and journal diagnostics sections are
// not produced there: a host without systemd produces no section (launchd and
// the unified log would need readers of their own).

#include <stdexcept>
#include <string>
#include <vector>

#include "diagnostics_thread.hpp"

namespace systemd_diagnostics {
namespace source {

bool has_systemd() { return false; }

std::string systemctl(const std::vector<std::string>&) { throw std::runtime_error("systemctl is not available on macOS"); }

std::string journalctl(const std::vector<std::string>&) { throw std::runtime_error("journalctl is not available on macOS"); }

std::int64_t monotonic_now_us() { return 0; }

}  // namespace source
}  // namespace systemd_diagnostics
