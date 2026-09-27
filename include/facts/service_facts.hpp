// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

namespace service_facts {
extern const char *const set_services;
extern const char *const id_installed;
extern const std::size_t max_services;

// Names match check_service: the SCM service name on Windows and the
// systemd Id without .service on Linux. Startup modes use the check's native
// vocabulary. No status, process information, accounts or command lines.
struct service {
  std::string name;
  std::string display_name;
  std::string start_type;
};

void publish(std::vector<service> services, std::time_t taken_at, nscapi::facts::response &out);
// Platform reader. Failure throws; an empty successful inventory is distinct.
std::vector<service> gather();
}  // namespace service_facts
