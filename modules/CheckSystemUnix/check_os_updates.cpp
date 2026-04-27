/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "check_os_updates.h"

#include <unistd.h>

#include <array>
#include <boost/algorithm/string.hpp>
#include <cstdio>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <str/xtos.hpp>

namespace os_updates {

std::string filter_obj::get_packages() const {
  std::string ret;
  for (const auto &p : packages) {
    if (!ret.empty()) ret += ", ";
    ret += p.name;
  }
  return ret;
}

std::string filter_obj::show() const {
  if (count == 0) return "no updates available";
  std::string ret = str::xtos(count) + " updates available";
  if (security > 0) ret += " (" + str::xtos(security) + " security)";
  return ret;
}

filter_obj_handler::filter_obj_handler() {
  registry_.add_string("manager", &filter_obj::get_manager, "Package manager used to query updates")
      .add_string("packages", &filter_obj::get_packages, "Comma separated list of available package updates");
  registry_.add_int_x("count", &filter_obj::get_count, "Total number of available updates")
      .add_int_perf("")
      .add_int_x("security", &filter_obj::get_security, "Number of available security updates")
      .add_int_perf("");
}

namespace {

// Execute a command via popen and capture stdout.
std::string run_command(const std::string &cmd) {
  std::array<char, 4096> buffer{};
  std::string result;
  const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) return "";
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

bool binary_exists(const std::string &path) { return access(path.c_str(), X_OK) == 0; }

}  // namespace

std::string detect_manager() {
  // Check standard install locations for each package manager.
  if (binary_exists("/usr/bin/apt-get") || binary_exists("/usr/local/bin/apt-get")) return "apt";
  if (binary_exists("/usr/bin/dnf") || binary_exists("/usr/local/bin/dnf")) return "dnf";
  if (binary_exists("/usr/bin/yum") || binary_exists("/usr/local/bin/yum")) return "yum";
  if (binary_exists("/usr/bin/zypper") || binary_exists("/usr/local/bin/zypper")) return "zypper";
  if (binary_exists("/usr/bin/pacman") || binary_exists("/usr/local/bin/pacman")) return "pacman";
  return "";
}

// Parse output of `apt list --upgradable 2>/dev/null`.
// Each upgradable line looks like:
//   pkg/jammy-security,jammy-security 1.2.3 amd64 [upgradable from: 1.2.2]
// First line is "Listing... Done" header which we skip.
filter_obj parse_apt_output(const std::string &output) {
  filter_obj obj;
  obj.manager = "apt";
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  for (const std::string &raw : lines) {
    std::string line = boost::trim_copy(raw);
    if (line.empty()) continue;
    if (boost::starts_with(line, "Listing")) continue;
    if (boost::starts_with(line, "WARNING")) continue;
    // pkg/source[,source...] version arch [upgradable from: oldver]
    auto slash = line.find('/');
    auto first_space = line.find(' ');
    if (slash == std::string::npos || first_space == std::string::npos || slash > first_space) continue;
    package_update p;
    p.name = line.substr(0, slash);
    std::string rest = line.substr(slash + 1);
    // Source string is up to the first space.
    auto rest_space = rest.find(' ');
    if (rest_space == std::string::npos) continue;
    p.source = rest.substr(0, rest_space);
    std::string after = boost::trim_copy(rest.substr(rest_space + 1));
    // Next token is the version.
    auto ver_space = after.find(' ');
    if (ver_space != std::string::npos) {
      p.version = after.substr(0, ver_space);
    } else {
      p.version = after;
    }
    // Sources containing -security mark this as a security update (Debian/Ubuntu convention).
    std::string lower_source = boost::to_lower_copy(p.source);
    p.security = lower_source.find("-security") != std::string::npos || lower_source.find("security.") != std::string::npos;
    obj.packages.push_back(p);
    if (p.security) obj.security++;
  }
  obj.count = static_cast<long long>(obj.packages.size());
  return obj;
}

// Parse output of `dnf -q check-update` / `yum -q check-update`.
// Lines: pkg.arch  version  repo
// Repo names ending in -security mark security updates (Fedora/RHEL convention).
// Lines starting with "Obsoleting" or "Security" headers are ignored.
filter_obj parse_dnf_output(const std::string &output) {
  filter_obj obj;
  obj.manager = "dnf";
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  bool in_obsoletes = false;
  for (const std::string &raw : lines) {
    std::string line = boost::trim_copy(raw);
    if (line.empty()) {
      in_obsoletes = false;
      continue;
    }
    // Headers / decorations start with non-package content.
    if (boost::starts_with(line, "Obsoleting") || boost::starts_with(line, "Last metadata") || boost::starts_with(line, "Security:") ||
        boost::starts_with(line, "Loaded plugins") || boost::starts_with(line, "Updating") || boost::starts_with(line, "Dependencies resolved")) {
      in_obsoletes = boost::starts_with(line, "Obsoleting");
      continue;
    }
    if (in_obsoletes) continue;
    // Continuation lines (start with spaces) belong to obsoletes.
    if (raw.size() && (raw[0] == ' ' || raw[0] == '\t')) continue;
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of(" \t"), boost::token_compress_on);
    if (parts.size() < 3) continue;
    package_update p;
    // First column is "name.arch"; strip the architecture suffix.
    auto dot = parts[0].rfind('.');
    if (dot != std::string::npos) {
      p.name = parts[0].substr(0, dot);
    } else {
      p.name = parts[0];
    }
    p.version = parts[1];
    p.source = parts[2];
    std::string lower_source = boost::to_lower_copy(p.source);
    p.security = lower_source.find("security") != std::string::npos;
    obj.packages.push_back(p);
    if (p.security) obj.security++;
  }
  obj.count = static_cast<long long>(obj.packages.size());
  return obj;
}

// Parse output of `zypper -q --non-interactive list-updates`.
// Lines look like:
//   v | repo | name | new version | arch
// First two lines are a header / separator that we skip.
filter_obj parse_zypper_output(const std::string &output) {
  filter_obj obj;
  obj.manager = "zypper";
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  for (const std::string &raw : lines) {
    std::string line = boost::trim_copy(raw);
    if (line.empty()) continue;
    if (line.find('|') == std::string::npos) continue;
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of("|"));
    for (auto &x : parts) boost::trim(x);
    if (parts.size() < 4) continue;
    if (parts[0] == "S" || parts[0] == "Status") continue;  // header row
    if (boost::starts_with(parts[0], "--")) continue;       // separator row
    package_update p;
    p.source = parts[1];
    p.name = parts[2];
    p.version = parts[3];
    std::string lower_source = boost::to_lower_copy(p.source);
    p.security = lower_source.find("security") != std::string::npos || lower_source.find("update") != std::string::npos;
    obj.packages.push_back(p);
    if (p.security) obj.security++;
  }
  obj.count = static_cast<long long>(obj.packages.size());
  return obj;
}

// Parse output of `pacman -Qu` (or `checkupdates`).
// Lines look like: name oldver -> newver
// pacman has no notion of "security" updates so all are general.
filter_obj parse_pacman_output(const std::string &output) {
  filter_obj obj;
  obj.manager = "pacman";
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  for (const std::string &raw : lines) {
    std::string line = boost::trim_copy(raw);
    if (line.empty()) continue;
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of(" \t"), boost::token_compress_on);
    if (parts.empty()) continue;
    package_update p;
    p.name = parts[0];
    if (parts.size() >= 4 && parts[2] == "->") {
      p.version = parts[3];
    } else if (parts.size() >= 2) {
      p.version = parts[1];
    }
    p.source = "pacman";
    p.security = false;
    obj.packages.push_back(p);
  }
  obj.count = static_cast<long long>(obj.packages.size());
  return obj;
}

filter_obj fetch_updates(const std::string &manager, const exec_fn &exec) {
  if (manager == "apt") {
    return parse_apt_output(exec("apt list --upgradable 2>/dev/null"));
  }
  if (manager == "dnf") {
    // dnf check-update exits 100 when updates are available; popen swallows the exit code.
    return parse_dnf_output(exec("dnf -q check-update 2>/dev/null"));
  }
  if (manager == "yum") {
    filter_obj obj = parse_dnf_output(exec("yum -q check-update 2>/dev/null"));
    obj.manager = "yum";
    return obj;
  }
  if (manager == "zypper") {
    return parse_zypper_output(exec("zypper -q --non-interactive list-updates 2>/dev/null"));
  }
  if (manager == "pacman") {
    return parse_pacman_output(exec("pacman -Qu 2>/dev/null"));
  }
  filter_obj obj;
  obj.manager = "none";
  return obj;
}

void check_os_updates(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef os_updates::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("count > 0", "security > 0", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${count} updates available (${security} security) via ${manager}",
                           "${count} updates (${security} security) via ${manager}", "updates", "", "%(status): No updates available.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  std::string manager = detect_manager();
  if (manager.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No supported package manager found (apt-get/dnf/yum/zypper/pacman)");
  }

  filter_obj result = fetch_updates(manager, run_command);
  boost::shared_ptr<filter_obj> record(new filter_obj(result));
  filter.match(record);

  filter_helper.post_process(filter);
}

}  // namespace os_updates
