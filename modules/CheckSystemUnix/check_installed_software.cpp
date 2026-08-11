// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_installed_software.h"

#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <boost/algorithm/string.hpp>
#include <cstdio>
#include <ctime>
#include <memory>
#include <parsers/filter/cli_helper.hpp>

namespace installed_software {

using parsers::where::type_date;
using parsers::where::type_size;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("name", &software_entry::get_name, "Package name")
      .add_string_var("version", &software_entry::get_version, "Version string (comparisons are lexical, not version-aware)")
      .add_string_var("publisher", &software_entry::get_publisher, "Maintainer (dpkg) / vendor (rpm); may be empty")
      .add_string_var("architecture", &software_entry::get_architecture, "Package architecture (amd64, x86_64, noarch, ...)")
      .add_string_var("manager", &software_entry::get_manager, "Package manager the entry came from (dpkg, rpm, pacman)")
      .add_string_var("status", &software_entry::get_status, "Package state; always 'installed' for listed packages")
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

long long file_mtime(const std::string &path) {
  struct stat st{};
  if (stat(path.c_str(), &st) != 0) return 0;
  return static_cast<long long>(st.st_mtime);
}

// "Name <email>" → "Name" (the email part adds noise to publisher filters).
std::string strip_email(const std::string &maintainer) {
  const std::size_t angle = maintainer.find(" <");
  return boost::trim_copy(angle == std::string::npos ? maintainer : maintainer.substr(0, angle));
}

}  // namespace

std::string detect_manager() {
  if (binary_exists("/usr/bin/dpkg-query") || binary_exists("/usr/local/bin/dpkg-query")) return "dpkg";
  if (binary_exists("/usr/bin/rpm") || binary_exists("/usr/local/bin/rpm")) return "rpm";
  if (binary_exists("/usr/bin/pacman") || binary_exists("/usr/local/bin/pacman")) return "pacman";
  return "";
}

// Parse dpkg-query -W output with the format
//   ${Package}\t${Version}\t${Architecture}\t${Maintainer}\t${Installed-Size}\t${Status}
// Status is three words ("install ok installed"); anything whose final word is
// not "installed" (config-files leftovers, half-configured) is skipped.
std::vector<software_entry> parse_dpkg_output(const std::string &output) {
  std::vector<software_entry> out;
  std::vector<std::string> lines;
  boost::split(lines, output, boost::is_any_of("\n"));
  for (const std::string &line : lines) {
    if (boost::trim_copy(line).empty()) continue;
    std::vector<std::string> parts;
    boost::split(parts, line, boost::is_any_of("\t"));
    if (parts.size() < 6) continue;
    if (!boost::ends_with(boost::trim_copy(parts[5]), "installed")) continue;
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

void apply_dpkg_install_dates(std::vector<software_entry> &entries, const mtime_fn &mtime) {
  for (software_entry &e : entries) {
    if (e.manager != "dpkg" || e.install_date_epoch > 0) continue;
    // Multi-arch packages use <name>:<arch>.list; older/same-arch ones plain <name>.list.
    long long t = 0;
    if (!e.architecture.empty()) t = mtime("/var/lib/dpkg/info/" + e.name + ":" + e.architecture + ".list");
    if (t == 0) t = mtime("/var/lib/dpkg/info/" + e.name + ".list");
    e.install_date_epoch = t;
    e.install_date_str = format_epoch_date(t);
  }
}

std::vector<software_entry> fetch_installed(const std::string &manager, const exec_fn &exec) {
  if (manager == "dpkg") {
    return parse_dpkg_output(
        exec("dpkg-query -W -f='${Package}\\t${Version}\\t${Architecture}\\t${Maintainer}\\t${Installed-Size}\\t${Status}\\n' 2>/dev/null"));
  }
  if (manager == "rpm") {
    return parse_rpm_output(exec("rpm -qa --qf '%{NAME}\\t%{VERSION}-%{RELEASE}\\t%{ARCH}\\t%{VENDOR}\\t%{SIZE}\\t%{INSTALLTIME}\\n' 2>/dev/null"));
  }
  if (manager == "pacman") {
    return parse_pacman_output(exec("pacman -Q 2>/dev/null"));
  }
  return {};
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
  const std::string manager = detect_manager();
  if (manager.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No supported package manager found (dpkg/rpm/pacman)");
  }

  std::vector<software_entry> entries = fetch_installed(manager, run_command);
  if (manager == "dpkg") apply_dpkg_install_dates(entries, file_mtime);

  check_from(request, response, entries);
}

}  // namespace installed_software
