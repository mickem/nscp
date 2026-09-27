// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <facts/service_facts.hpp>
#include <stdexcept>

namespace service_facts {
std::vector<service> gather() {
  // The launchd check enumerates loaded jobs, not every installed service.
  // Do not publish that partial view as an installed-service inventory.
  throw std::runtime_error("services.installed is not supported on macOS");
}
}  // namespace service_facts
