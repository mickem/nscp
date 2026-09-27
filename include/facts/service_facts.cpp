// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <facts/service_facts.hpp>
#include <nscapi/nscapi_facts_helper.hpp>
#include <tuple>

namespace service_facts {
const char *const set_services = "services";
const char *const id_installed = "services.installed";
const std::size_t max_services = 2500;

void publish(std::vector<service> services, const std::time_t taken_at, nscapi::facts::response &out) {
  std::sort(services.begin(), services.end(),
            [](const service &a, const service &b) { return std::tie(a.name, a.display_name, a.start_type) < std::tie(b.name, b.display_name, b.start_type); });
  services.erase(std::remove_if(services.begin(), services.end(), [](const service &s) { return s.name.empty(); }), services.end());
  // systemd aliases resolve to the same canonical Id.
  services.erase(std::unique(services.begin(), services.end(), [](const service &a, const service &b) { return a.name == b.name; }), services.end());
  nscapi::facts::record_list list = out.set(set_services).list("installed");
  for (std::size_t i = 0; i < std::min(services.size(), max_services); ++i) {
    const service &s = services[i];
    list.record(s.name).value("name", s.name).value("display_name", s.display_name).value("start_type", s.start_type);
  }
  if (services.size() > max_services)
    out.error(set_services, "Service inventory truncated: found " + std::to_string(services.size()) + ", reporting " + std::to_string(max_services));
  out.gathered(set_services, taken_at);
}
}  // namespace service_facts
