// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_installed_software.h"

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <boost/algorithm/string.hpp>
#include <cstdio>
#include <ctime>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/helpers.hpp>

namespace installed_software {

using parsers::where::type_date;
using parsers::where::type_size;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("name", &software_entry::get_name, "Package name")
      .add_string_var("version", &software_entry::get_version, "Version string (rpm: version-release); comparisons are lexical, not version-aware")
      .add_string_var("publisher", &software_entry::get_publisher, "Maintainer (dpkg, email stripped) / vendor (rpm); empty for pacman")
      .add_string_var("architecture", &software_entry::get_architecture, "Package architecture (amd64, x86_64, noarch, ...)")
      .add_string_var("manager", &software_entry::get_manager, "Package manager the entry came from (dpkg, rpm, pacman)")
      .add_string_var("package_status", &software_entry::get_status, "Package state; always 'installed' for listed packages")
      .add_string_var("status", &software_entry::get_status, "Deprecated alias for package_status (the name clashes with the generic status summary keyword).")
      .add_string_var("install_date_s", &software_entry::get_install_date_str, "Install date as YYYY-MM-DD; empty when unknown");
  registry_.add_int_var("install_date", type_date, &software_entry::get_install_date,
                        "Install date (supports date expressions such as 'install_date > -30d'); unset when the manager does not record one")
      .add_int_var("size", type_size, &software_entry::get_size, "Installed size in bytes; 0 when not recorded");
  // clang-format on
}

std::string format_epoch_date(const long long epoch) {
  if (epoch <= 0) return "";
  const time_t t = static_cast<time_t>(epoch);
  struct tm parts{};
  if (gmtime_r(&t, &parts) == nullptr) return "";
  char buf[16];
  if (strftime(buf, sizeof(buf), "%Y-%m-%d", &parts) == 0) return "";
  return buf;
}

namespace {

// Execute a command via popen, capture stdout and keep the exit status: an
// empty package list from a failed query must not be mistaken for an empty
// package database.
command_result run_command(const std::string &cmd) {
  std::array<char, 4096> buffer{};
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) return command_result("", false);
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  const int status = pclose(pipe);
  // pclose returns -1 on failure, otherwise a wait(2) status: only a normal
  // exit with code 0 counts as a successful query.
  const bool ok = status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  return command_result(result, ok);
}

bool binary_exists(const std::string &path) { return access(path.c_str(), X_OK) == 0; }

// "Name <email>" → "Name" (the email part adds noise to publisher filters).
std::string strip_email(const std::string &maintainer) {
  const std::size_t angle = maintainer.find(" <");
  return boost::trim_copy(angle == std::string::npos ? maintainer : maintainer.substr(0, angle));
}

// First existing candidate, or empty when the binary is not installed.
std::string first_existing(const std::string &a, const std::string &b) {
  if (binary_exists(a)) return a;
  if (binary_exists(b)) return b;
  return "";
}

}  // namespace

package_manager detect_manager() {
  package_manager pm;
  // Commands are run by the absolute path probed here rather than by bare
  // name, so the collector cannot be redirected through an inherited PATH.
  pm.binary = first_existing("/usr/bin/dpkg-query", "/usr/local/bin/dpkg-query");
  if (!pm.binary.empty()) {
    pm.name = "dpkg";
    return pm;
  }
  pm.binary = first_existing("/usr/bin/rpm", "/usr/local/bin/rpm");
  if (!pm.binary.empty()) {
    pm.name = "rpm";
    return pm;
  }
  pm.binary = first_existing("/usr/bin/pacman", "/usr/local/bin/pacman");
  if (!pm.binary.empty()) {
    pm.name = "pacman";
    return pm;
  }
  return package_manager();
}

// Parse dpkg-query -W output with the format
//   ${Package}\t${Version}\t${Architecture}\t${Maintainer}\t${Installed-Size}\t${Status}\t${db-fsys:Last-Modified}
// Status is three words ("<desired> <error> <state>"). Only a state of exactly
// "installed" counts: the states that merely end in it ("not-installed",
// "half-installed") are removed or broken packages, not installed ones.
std::vector<software_entry> parse_dpkg_output(const std::string &output) {
  std::vector<software_entry> out;
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  for (const std::string &line : lines) {
    if (boost::trim_copy(line).empty()) continue;
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of("\t"));
    if (parts.size() < 6) continue;
    std::vector<std::string> status_words;
    boost::split(status_words, boost::trim_copy(parts[5]), boost::is_any_of(" \t"), boost::token_compress_on);
    // The desired state (hold, deinstall, ...) does not change the fact that
    // the package is currently on disk, so only the current state is checked.
    if (status_words.empty() || status_words.back() != "installed") continue;
    software_entry e;
    e.manager = "dpkg";
    e.name = boost::trim_copy(parts[0]);
    e.version = boost::trim_copy(parts[1]);
    e.architecture = boost::trim_copy(parts[2]);
    e.publisher = strip_email(parts[3]);
    try {
      // Installed-Size is in KiB.
      e.size_bytes = std::stoll(boost::trim_copy(parts[4])) * 1024LL;
    } catch (...) {
      e.size_bytes = 0;
    }
    // The install date is the last field. dpkg-query grew db-fsys:Last-Modified
    // in 1.19.3; an older one leaves the column empty, which reads as "unknown"
    // just like a manager that records no date at all.
    if (parts.size() >= 7) {
      try {
        e.install_date_epoch = std::stoll(boost::trim_copy(parts[6]));
      } catch (...) {
        e.install_date_epoch = 0;
      }
      e.install_date_str = format_epoch_date(e.install_date_epoch);
    }
    if (e.name.empty()) continue;
    out.push_back(e);
  }
  return out;
}

// Parse rpm -qa output with the query format
//   %{NAME}\t%{VERSION}-%{RELEASE}\t%{ARCH}\t%{VENDOR}\t%{SIZE}\t%{INSTALLTIME}
std::vector<software_entry> parse_rpm_output(const std::string &output) {
  std::vector<software_entry> out;
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  for (const std::string &line : lines) {
    if (boost::trim_copy(line).empty()) continue;
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of("\t"));
    if (parts.size() < 6) continue;
    software_entry e;
    e.manager = "rpm";
    e.name = boost::trim_copy(parts[0]);
    e.version = boost::trim_copy(parts[1]);
    e.architecture = boost::trim_copy(parts[2]);
    e.publisher = boost::trim_copy(parts[3]);
    if (e.publisher == "(none)") e.publisher = "";
    try {
      e.size_bytes = std::stoll(boost::trim_copy(parts[4]));
    } catch (...) {
      e.size_bytes = 0;
    }
    try {
      e.install_date_epoch = std::stoll(boost::trim_copy(parts[5]));
    } catch (...) {
      e.install_date_epoch = 0;
    }
    e.install_date_str = format_epoch_date(e.install_date_epoch);
    if (e.name.empty()) continue;
    out.push_back(e);
  }
  return out;
}

// Parse pacman -Q output ("name version" per line). pacman -Q does not expose
// vendor/size/date without the much heavier -Qi format, so those stay unset.
std::vector<software_entry> parse_pacman_output(const std::string &output) {
  std::vector<software_entry> out;
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  for (const std::string &raw : lines) {
    const std::string line = boost::trim_copy(raw);
    if (line.empty()) continue;
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of(" \t"), boost::token_compress_on);
    if (parts.empty() || parts[0].empty()) continue;
    software_entry e;
    e.manager = "pacman";
    e.name = parts[0];
    if (parts.size() >= 2) e.version = parts[1];
    out.push_back(e);
  }
  return out;
}

fetch_result fetch_installed(const package_manager &manager, const exec_fn &exec) {
  fetch_result result;
  if (manager.name == "dpkg") {
    const command_result r =
        exec(manager.binary +
             " -W -f='${Package}\\t${Version}\\t${Architecture}\\t${Maintainer}\\t${Installed-Size}\\t${Status}\\t${db-fsys:Last-Modified}\\n' 2>/dev/null");
    result.ok = r.ok;
    if (r.ok) result.entries = parse_dpkg_output(r.output);
    return result;
  }
  if (manager.name == "rpm") {
    const command_result r =
        exec(manager.binary + " -qa --qf '%{NAME}\\t%{VERSION}-%{RELEASE}\\t%{ARCH}\\t%{VENDOR}\\t%{SIZE}\\t%{INSTALLTIME}\\n' 2>/dev/null");
    result.ok = r.ok;
    if (r.ok) result.entries = parse_rpm_output(r.output);
    return result;
  }
  if (manager.name == "pacman") {
    const command_result r = exec(manager.binary + " -Q 2>/dev/null");
    result.ok = r.ok;
    if (r.ok) result.entries = parse_pacman_output(r.output);
    return result;
  }
  return result;
}

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::vector<software_entry> &entries) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter filter_;
  // No default thresholds: a bare call is an inventory (OK + count perf);
  // unwanted/EOL policy comes in via warn=/crit= expressions, and an empty
  // match set (an absent-unwanted-software probe) is OK. Mirrors the Windows
  // check_installed_software contract.
  filter_helper.add_options("", "", "", filter_.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_list}", "${name} ${version} (${publisher})", "${name}", "%(status): No installed software found",
                           "%(status): %(count) software packages installed.");
  // Thresholding on install_date must not spray a meaningless epoch-seconds
  // perf series per package; the aggregate count is the useful perf value.
  filter_helper.set_default_perf_config("install_date(ignored:true)extra(count)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter_)) return;

  parsers::where::constants::reset();
  for (const software_entry &e : entries) {
    const std::shared_ptr<software_entry> record(new software_entry(e));
    filter_.match(record);
    if (filter_.has_errors()) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Filter error: " + filter_.get_errors());
    }
  }
  filter_helper.post_process(filter_);
}

void check_installed_software(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  const package_manager manager = detect_manager();
  if (manager.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No supported package manager found (dpkg/rpm/pacman)");
  }

  fetch_result fetched = fetch_installed(manager, run_command);
  if (!fetched.ok) {
    // A failed query yields an empty list; reporting that as "no installed
    // software found" would turn a broken package database into a clean OK.
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to query installed software from " + manager.name + " (" + manager.binary + ")");
  }
  check_from(request, response, fetched.entries);
}

}  // namespace installed_software
