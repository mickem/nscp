// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_os_updates.h"

#include <unistd.h>

#include <array>
#include <boost/algorithm/string.hpp>
#include <cstdio>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <str/xtos.hpp>

#include "exec_command.h"

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
  registry_.add_string_var("manager", &filter_obj::get_manager, "Package manager used to query updates (apt, dnf, yum, zypper, pacman; softwareupdate on macOS)")
      .add_string_var("packages", &filter_obj::get_packages, "Comma separated list of available package updates");
  registry_.add_int_var("updates", &filter_obj::get_count, "Total number of available updates")
      .add_int_perf("")
      .add_int_var("count", &filter_obj::get_count, "Deprecated alias for updates (the name clashes with the generic count summary keyword)")
      .add_int_perf("")
      .add_int_var("security", &filter_obj::get_security,
                   "Number of available security updates (on macOS the updates named as security responses; a macOS point release is not counted)")
      .add_int_perf("", "", "_security");
  registry_.add_optional_int_var("last_checked", parsers::where::type_date, [](auto obj) { return obj->get_last_checked(); }, "unknown",
                                 "When the update list was last refreshed from the update server (macOS: the last successful background check); "
                                 "supports date expressions such as 'last_checked < -7d'. 'unknown' where the package manager does not record it")
      .no_perf();
}

namespace {

// Execute a command via popen and capture stdout.
std::string run_command(const std::string &cmd) {
  std::array<char, 4096> buffer{};
  std::string result;
  using pclose_fn_t = int (*)(FILE *);
  const std::unique_ptr<FILE, pclose_fn_t> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) return "";
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

bool binary_exists(const std::string &path) { return access(path.c_str(), X_OK) == 0; }

}  // namespace

std::string detect_manager() {
  // Check standard install locations for each package manager. softwareupdate
  // is part of macOS and of nothing else, and goes first because a Mac can
  // carry a Homebrew copy of one of the Linux managers.
  if (binary_exists("/usr/sbin/softwareupdate")) return "softwareupdate";
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

namespace {
bool names_security(const std::string &text) {
  const std::string lower = boost::to_lower_copy(text);
  return lower.find("security") != std::string::npos;
}

// A Rapid Security Response is versioned as its release plus a letter,
// "13.4.1 (a)", and is often titled as a plain OS update.
bool is_rapid_response(const std::string &version) {
  const std::string v = boost::trim_copy(version);
  return v.size() >= 4 && v[v.size() - 1] == ')' && v[v.size() - 3] == '(' && v[v.size() - 4] == ' ' && v[v.size() - 2] >= 'a' && v[v.size() - 2] <= 'z';
}

bool is_security_update(const package_update &p) { return names_security(p.name) || names_security(p.source) || is_rapid_response(p.version); }
}  // namespace

filter_obj parse_software_update_plist(const plist::value &plist) {
  filter_obj obj;
  obj.manager = "softwareupdate";
  obj.last_checked = plist["LastSuccessfulDate"].as_date();
  const plist::value &updates = plist["RecommendedUpdates"];
  if (updates.kind == plist::value::array) {
    for (const plist::value &u : updates.items) {
      package_update p;
      p.name = u["Display Name"].as_string(u["Identifier"].as_string());
      if (p.name.empty()) continue;
      p.version = u["Display Version"].as_string();
      p.source = u["Identifier"].as_string(u["Product Key"].as_string());
      p.security = is_security_update(p);
      obj.packages.push_back(p);
      if (p.security) obj.security++;
    }
  }
  obj.count = static_cast<long long>(obj.packages.size());
  return obj;
}

// Parse `softwareupdate --list`. Since macOS 10.15 each update is
//   * Label: macOS Sonoma 14.5-23F79
//   \tTitle: macOS Sonoma 14.5, Version: 14.5, Size: 6834924KiB, Recommended: YES, Action: restart,
// and before that
//   * Safari12.1.1Mojave-12.1.1
//   \tSafari (12.1.1), 67140K [recommended]
// The progress lines ("Finding available software") and "No new software
// available." carry no leading "*" and are ignored.
filter_obj parse_softwareupdate_output(const std::string &output) {
  filter_obj obj;
  obj.manager = "softwareupdate";
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const std::string line = boost::trim_copy(lines[i]);
    if (line.empty() || line[0] != '*') continue;
    std::string label = boost::trim_copy(line.substr(1));
    if (boost::starts_with(label, "Label:")) label = boost::trim_copy(label.substr(6));
    if (label.empty()) continue;
    package_update p;
    p.source = label;
    p.name = label;
    const std::string detail = i + 1 < lines.size() ? boost::trim_copy(lines[i + 1]) : std::string();
    if (boost::starts_with(detail, "Title:")) {
      std::vector<std::string> fields;
      boost::split(fields, detail, boost::is_any_of(","));
      for (std::string &field : fields) {
        boost::trim(field);
        if (boost::starts_with(field, "Title:")) p.name = boost::trim_copy(field.substr(6));
        if (boost::starts_with(field, "Version:")) p.version = boost::trim_copy(field.substr(8));
      }
    } else if (!detail.empty() && detail[0] != '*') {
      const std::string::size_type open = detail.find(" (");
      const std::string::size_type close = detail.find(')', open == std::string::npos ? 0 : open);
      if (open != std::string::npos && close != std::string::npos) {
        p.name = detail.substr(0, open);
        p.version = detail.substr(open + 2, close - open - 2);
      }
    }
    p.security = is_security_update(p);
    obj.packages.push_back(p);
    if (p.security) obj.security++;
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
  if (manager == "softwareupdate") {
    // The cached list; the live query is the check's live=true, which needs
    // a deadline the exec_fn here does not have.
    return parse_software_update_plist(read_software_update_cache());
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
  bool live = false;
  filter_helper.add_options("updates > 0", "security > 0", "", filter.get_filter_syntax(), "ok");
  // Top-syntax renders after the record is detached, so record variables like
  // ${updates} read as 0 there (and the generic ${count} is the matched-row
  // count) -- the real numbers render through the detail line via ${list}.
  filter_helper.add_syntax("${status}: ${list}", "${updates} updates available (${security} security) via ${manager}", "updates", "",
                           "%(status): No updates available.");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("live", boost::program_options::value<bool>(&live)->implicit_value(true)->default_value(false),
     "macOS: ask Apple's update server with `softwareupdate --list` (10 to 60 seconds, needs network access) instead of reading the list macOS "
     "cached at its last background check. Ignored on Linux, where the package manager is always queried.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  std::string manager = detect_manager();
  if (manager.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No supported package manager found (apt-get/dnf/yum/zypper/pacman)");
  }

  filter_obj result;
  if (manager == "softwareupdate" && live) {
    const system_exec::exec_result r = system_exec::run({"/usr/sbin/softwareupdate", "--list"}, 60000);
    if (!r.started) return nscapi::protobuf::functions::set_response_bad(*response, "Failed to run /usr/sbin/softwareupdate");
    if (r.timed_out) return nscapi::protobuf::functions::set_response_bad(*response, "softwareupdate --list did not finish within 60 seconds");
    if (r.exit_code != 0) {
      return nscapi::protobuf::functions::set_response_bad(*response, "softwareupdate --list failed with exit code " + str::xtos(r.exit_code));
    }
    result = parse_softwareupdate_output(r.output);
  } else if (manager == "softwareupdate") {
    const plist::value cache = read_software_update_cache();
    // No cache is not "no updates": the background check has not run yet, or
    // the file is not readable.
    if (cache.empty()) {
      return nscapi::protobuf::functions::set_response_bad(
          *response, "No cached update list in /Library/Preferences/com.apple.SoftwareUpdate.plist (use live=true to ask the update server)");
    }
    result = parse_software_update_plist(cache);
  } else {
    result = fetch_updates(manager, run_command);
  }
  std::shared_ptr<filter_obj> record(new filter_obj(result));
  filter.match(record);

  filter_helper.post_process(filter);
}

}  // namespace os_updates
