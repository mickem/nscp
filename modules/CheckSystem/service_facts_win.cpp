// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <facts/service_facts.hpp>
#include <win/services.hpp>

namespace service_facts {
std::vector<service> gather() {
  std::vector<service> result;
  // Strict configuration reads: a failed query must not masquerade as a
  // boot-start service or replace the last complete inventory.
  for (const auto &s : win_list_services::enum_services("", SERVICE_WIN32, SERVICE_STATE_ALL, {}, true)) {
    result.push_back({s.name, s.displayname, s.get_start_type_s()});
  }
  return result;
}
}  // namespace service_facts
