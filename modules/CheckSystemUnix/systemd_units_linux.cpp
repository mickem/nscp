// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "systemd_units_linux.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace systemd_units {
std::vector<std::string> list_services(const command_runner &run) {
  std::vector<std::string> names;
  std::istringstream lines(run({"list-units", "--type=service", "--all", "--no-legend", "--plain", "--no-pager"}));
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream row(line);
    std::string name, load_state, active, sub;
    if (!(row >> name)) continue;
    if (!checks::check_svc_filter::is_safe_unit_name(name) || name.size() <= 8 || name.compare(name.size() - 8, 8, ".service") != 0 ||
        !(row >> load_state >> active >> sub))
      throw std::runtime_error("Invalid systemctl list-units row: " + line);
    // Absent optional dependencies are references, not installed services.
    if (load_state != "not-found") names.push_back(name);
  }
  return names;
}

std::vector<checks::check_svc_filter::filter_obj> show_services(const std::vector<std::string> &names, const command_runner &run, const std::string &properties,
                                                                const bool require_complete) {
  std::vector<checks::check_svc_filter::filter_obj> result;
  for (std::size_t begin = 0; begin < names.size(); begin += 128) {
    const std::size_t end = std::min(begin + 128, names.size());
    std::vector<std::string> argv = {"show", "--no-pager"};
    if (!properties.empty()) argv.push_back("--property=" + properties);
    argv.push_back("--");
    argv.insert(argv.end(), names.begin() + begin, names.begin() + end);
    const auto rows = checks::check_svc_filter::parse_systemctl_show(run(argv));
    if (require_complete && rows.size() != end - begin) throw std::runtime_error("Incomplete systemctl show response");
    for (const auto &row : rows) {
      if (require_complete && (row.name.empty() || row.load_state.empty())) throw std::runtime_error("Incomplete systemctl service metadata");
      result.push_back(row);
    }
  }
  return result;
}
}  // namespace systemd_units
