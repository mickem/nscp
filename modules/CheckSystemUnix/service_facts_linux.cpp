// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "service_facts_linux.hpp"

#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include "check_service.h"
#include "exec_command.h"

namespace service_facts {
namespace {
bool is_service(const std::string &name) { return name.size() > 8 && name.compare(name.size() - 8, 8, ".service") == 0; }
bool is_template(const std::string &name) { return name.size() > 9 && name.compare(name.size() - 9, 9, "@.service") == 0; }
std::string short_name(const std::string &name) { return name.substr(0, name.size() - 8); }
}  // namespace

std::vector<service> gather_systemd(const command_runner &run) {
  // list-units alone omits installed services that have never been loaded.
  // Also include loaded instances and transient services absent from disk.
  std::map<std::string, std::string> names;
  std::istringstream files(run({"list-unit-files", "--type=service", "--no-legend", "--no-pager"}));
  std::string line, name, state;
  while (std::getline(files, line)) {
    std::istringstream row(line);
    if (!(row >> name)) continue;
    if (!is_service(name) || !(row >> state)) throw std::runtime_error("Invalid systemctl list-unit-files response");
    names[name] = state;
  }
  for (const auto &name : systemd_units::list_services(run)) {
    names.emplace(name, "");
  }

  std::vector<service> result;
  std::set<std::string> templates;
  std::vector<std::string> ordinary;
  for (const auto &entry : names) {
    // Bare templates cannot be queried with show. Resolve every unmasked
    // template via cat: older systemd versions do not report the alias state.
    // A masked unit has no readable fragment and keeps its listed identity.
    if (is_template(entry.first)) {
      std::string canonical = entry.first;
      if (entry.second != "masked" && entry.second != "masked-runtime") {
        std::istringstream definition(run({"cat", "--no-pager", "--", entry.first}));
        std::string header;
        if (!std::getline(definition, header) || header.compare(0, 3, "# /") != 0)
          throw std::runtime_error("Could not resolve systemd template alias: " + entry.first);
        canonical = header.substr(header.find_last_of('/') + 1);
        const auto target = names.find(canonical);
        if (!is_template(canonical) || target == names.end() || target->second.empty() || target->second == "alias")
          throw std::runtime_error("Missing or unresolved systemd template alias target: " + entry.first);
      }
      if (templates.insert(canonical).second) result.push_back({short_name(canonical), "", names.at(canonical)});
    } else {
      ordinary.push_back(entry.first);
    }
  }
  for (const auto &row : systemd_units::show_services(ordinary, run, "Id,Description,UnitFileState,LoadState", true)) {
    // Stale aliases and units removed since listing are not installed services.
    if (row.load_state == "not-found") continue;
    // A unit with error/bad-setting is still installed; retain its metadata.
    result.push_back({row.name, row.desc, row.start_type});
  }
  return result;
}

std::vector<service> gather() {
  return gather_systemd([](const std::vector<std::string> &args) {
    std::vector<std::string> argv = {"systemctl"};
    argv.insert(argv.end(), args.begin(), args.end());
    return system_exec::run_inventory_command(argv);
  });
}
}  // namespace service_facts
