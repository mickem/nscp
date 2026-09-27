// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "service_facts_linux.hpp"

#include <unistd.h>

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
  std::istringstream units(run({"list-units", "--type=service", "--all", "--no-legend", "--plain", "--no-pager"}));
  while (std::getline(units, line)) {
    std::istringstream row(line);
    if (!(row >> name)) continue;
    std::string load_state;
    if (!is_service(name) || !(row >> load_state)) throw std::runtime_error("Invalid systemctl list-units response");
    // Dependencies on absent optional units appear in list-units --all as
    // not-found. They are references, not installed or transient services.
    if (load_state == "not-found") continue;
    names.emplace(name, "");
  }

  std::vector<service> result;
  std::set<std::string> templates;
  std::vector<std::string> batch;
  const auto flush = [&]() {
    if (batch.empty()) return;
    std::vector<std::string> argv = {"show", "--no-pager", "--property=Id,Description,UnitFileState,LoadState", "--"};
    argv.insert(argv.end(), batch.begin(), batch.end());
    const auto rows = checks::check_svc_filter::parse_systemctl_show(run(argv));
    if (rows.size() != batch.size()) throw std::runtime_error("Incomplete systemctl show response");
    for (const auto &row : rows) {
      if (row.name.empty() || row.load_state.empty() || row.load_state == "not-found" || row.load_state == "error" || row.load_state == "bad-setting")
        throw std::runtime_error("Service disappeared or could not be read during inventory collection");
      result.push_back({row.name, row.desc, row.start_type});
    }
    batch.clear();
  };
  for (const auto &entry : names) {
    // Bare templates cannot be queried with show. For aliases, cat follows
    // the alias chain and names the canonical fragment in its first header.
    // Keep the canonical unit-file state, not the alias's "alias" state.
    if (is_template(entry.first)) {
      std::string canonical = entry.first;
      if (entry.second == "alias") {
        std::istringstream definition(run({"cat", "--no-pager", "--", entry.first}));
        std::string header;
        if (!std::getline(definition, header) || header.compare(0, 3, "# /") != 0)
          throw std::runtime_error("Could not resolve systemd template alias: " + entry.first);
        canonical = header.substr(header.find_last_of('/') + 1);
        const auto target = names.find(canonical);
        if (!is_template(canonical) || canonical == entry.first || target == names.end() || target->second.empty() || target->second == "alias")
          throw std::runtime_error("Missing or unresolved systemd template alias target: " + entry.first);
      }
      if (templates.insert(canonical).second) result.push_back({short_name(canonical), "", names.at(canonical)});
    } else {
      batch.push_back(entry.first);
      if (batch.size() == 128) flush();
    }
  }
  flush();
  return result;
}

std::vector<service> gather() {
  std::string binary;
  for (const char *candidate : {"/usr/bin/systemctl", "/bin/systemctl"}) {
    if (access(candidate, X_OK) == 0) {
      binary = candidate;
      break;
    }
  }
  if (binary.empty()) throw std::runtime_error("Service facts require systemd (systemctl was not found)");
  return gather_systemd([&](const std::vector<std::string> &args) {
    std::vector<std::string> argv = {binary};
    argv.insert(argv.end(), args.begin(), args.end());
    return system_exec::run_inventory_command(argv);
  });
}
}  // namespace service_facts
