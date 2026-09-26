// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Parsing launchctl's output into check_service rows. Pure and
// platform-neutral, so the tests pin it on every platform against captured
// output; service_source_darwin.cpp runs launchctl and feeds it here.
//
// Apple documents `launchctl print` as unstable and not meant for parsing,
// so the parsers read as little as they can: the one `services` table, the
// top-level properties of one job, and the disabled map. Anything unexpected
// is skipped rather than failing the check.

#include <boost/algorithm/string.hpp>
#include <sstream>

#include "check_service.h"

namespace checks {
namespace check_svc_filter {

namespace {

// "-" (never exited / not running) is absent; anything numeric is a value.
bool parse_number(const std::string &token, long long &out) {
  if (token.empty() || token == "-") return false;
  try {
    std::size_t used = 0;
    out = std::stoll(token, &used);
    return used == token.size();
  } catch (...) {
    return false;
  }
}

// Brace depth of a line: +1 for an opening block, -1 for a closing one.
int depth_change(const std::string &trimmed) {
  if (!trimmed.empty() && trimmed.back() == '{') return 1;
  if (!trimmed.empty() && trimmed.front() == '}') return -1;
  return 0;
}

}  // namespace

std::vector<launchd_listing> parse_launchctl_services(const std::string &output) {
  std::vector<launchd_listing> result;
  std::istringstream lines(output);
  std::string line;
  bool in_services = false;
  int depth = 0;
  while (std::getline(lines, line)) {
    const std::string trimmed = boost::trim_copy(line);
    if (!in_services) {
      // "services = {" at the domain's top level (depth 1 inside "system = {").
      if (trimmed == "services = {") {
        in_services = true;
        depth = 1;
      }
      continue;
    }
    depth += depth_change(trimmed);
    if (depth <= 0) break;
    std::vector<std::string> tokens;
    boost::split(tokens, trimmed, boost::is_any_of(" \t"), boost::token_compress_on);
    if (tokens.size() < 3) continue;
    launchd_listing job;
    job.label = tokens.back();
    long long pid = 0;
    if (parse_number(tokens[0], pid) && pid > 0) job.pid = pid;
    job.has_exit = parse_number(tokens[1], job.last_exit);
    if (job.label.empty() || !is_safe_unit_name(job.label)) continue;
    result.push_back(job);
  }
  return result;
}

std::map<std::string, bool> parse_launchctl_disabled(const std::string &output) {
  std::map<std::string, bool> result;
  std::istringstream lines(output);
  std::string line;
  bool in_block = false;
  while (std::getline(lines, line)) {
    const std::string trimmed = boost::trim_copy(line);
    if (!in_block) {
      if (trimmed == "disabled services = {") in_block = true;
      continue;
    }
    if (!trimmed.empty() && trimmed.front() == '}') break;
    // "com.apple.ftpd" => disabled
    const std::string::size_type arrow = trimmed.find("=>");
    if (arrow == std::string::npos) continue;
    std::string label = boost::trim_copy(trimmed.substr(0, arrow));
    const std::string value = boost::to_lower_copy(boost::trim_copy(trimmed.substr(arrow + 2)));
    if (label.size() >= 2 && label.front() == '"' && label.back() == '"') label = label.substr(1, label.size() - 2);
    if (label.empty()) continue;
    if (value == "disabled" || value == "true") {
      result[label] = true;
    } else if (value == "enabled" || value == "false") {
      result[label] = false;
    }
  }
  return result;
}

std::map<std::string, std::string> parse_launchctl_print(const std::string &output) {
  std::map<std::string, std::string> result;
  std::istringstream lines(output);
  std::string line;
  int depth = 0;
  while (std::getline(lines, line)) {
    const std::string trimmed = boost::trim_copy(line);
    if (trimmed.empty()) continue;
    const int change = depth_change(trimmed);
    // Top-level properties are the lines directly inside the job's own block.
    if (depth == 1 && change == 0) {
      const std::string::size_type eq = trimmed.find(" = ");
      if (eq != std::string::npos) {
        result[boost::trim_copy(trimmed.substr(0, eq))] = boost::trim_copy(trimmed.substr(eq + 3));
      }
    }
    depth += change;
    if (depth < 0) depth = 0;
  }
  return result;
}

filter_obj launchd_row(const launchd_listing &job, const std::map<std::string, bool> &disabled, const std::map<std::string, std::string> &properties) {
  filter_obj info;
  info.name = job.label;
  info.load_state = "loaded";

  long long pid = job.pid;
  bool has_exit = job.has_exit;
  long long last_exit = job.last_exit;
  const auto prop = [&properties](const std::string &key) {
    const auto it = properties.find(key);
    return it == properties.end() ? std::string() : it->second;
  };
  if (!properties.empty()) {
    // The job's own print is newer than the listing and wins where it speaks.
    long long printed = 0;
    if (parse_number(prop("pid"), printed) && printed > 0) pid = printed;
    // "last exit code = 78: Function not implemented" or "(never exited)".
    const std::string exit_text = prop("last exit code");
    if (!exit_text.empty()) {
      const std::string code = exit_text.substr(0, exit_text.find(':'));
      long long parsed = 0;
      if (parse_number(boost::trim_copy(code), parsed)) {
        has_exit = true;
        last_exit = parsed;
      } else if (exit_text.find("never exited") != std::string::npos) {
        has_exit = false;
      }
    }
    info.desc = prop("program");
    if (info.desc.empty()) info.desc = prop("path");
  }
  info.pid = pid > 0 ? static_cast<int>(pid) : 0;

  // Start type: an explicit override first, then how the job is launched
  // when its properties are known.
  const auto dis = disabled.find(job.label);
  if (dis != disabled.end() && dis->second) {
    info.start_type = "disabled";
  } else if (!properties.empty()) {
    std::vector<std::string> flags;
    boost::split(flags, prop("properties"), boost::is_any_of("|"));
    bool at_load = false;
    for (std::string &flag : flags) {
      boost::trim(flag);
      if (flag == "runatload" || flag == "keepalive") at_load = true;
    }
    info.start_type = at_load ? "enabled" : "on-demand";
  } else {
    info.start_type = "enabled";
  }

  if (info.pid > 0) {
    info.active = "active";
    info.sub_state = "running";
    info.state = "running";
  } else if (has_exit && last_exit > 0) {
    // A positive status is the program's own exit code; a negative one is
    // the signal launchd stopped it with, which is how an idle on-demand job
    // normally ends.
    info.active = "failed";
    info.sub_state = "failed";
    info.state = "stopped";
  } else {
    info.active = "inactive";
    info.sub_state = "dead";
    info.state = info.is_on_demand() ? "static" : "stopped";
  }
  return info;
}

}  // namespace check_svc_filter
}  // namespace checks
