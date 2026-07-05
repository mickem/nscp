// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_os_version.h"

#include <sys/utsname.h>

#include <boost/algorithm/string/trim.hpp>
#include <fstream>
#include <map>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>
#include <string>

namespace os_version {

os_release_info parse_os_release(const std::string &content) {
  os_release_info out;
  std::map<std::string, std::string> kv;
  std::istringstream lines(content);
  std::string line;
  while (std::getline(lines, line)) {
    boost::algorithm::trim(line);
    if (line.empty() || line[0] == '#') continue;
    const std::string::size_type eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    boost::algorithm::trim(key);
    boost::algorithm::trim(val);
    // Strip a single pair of surrounding quotes (os-release quoting).
    if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
      val = val.substr(1, val.size() - 2);
    }
    kv[key] = val;
  }

  out.distribution = kv.count("ID") ? kv["ID"] : "";
  out.distribution_name = kv.count("NAME") ? kv["NAME"] : "";
  out.version = kv.count("VERSION_ID") ? kv["VERSION_ID"] : "";
  out.pretty = kv.count("PRETTY_NAME") ? kv["PRETTY_NAME"] : "";
  // ID_LIKE is a space-separated list of parent distros; take the first as the
  // family, falling back to ID (a distro is its own family).
  if (kv.count("ID_LIKE") && !kv["ID_LIKE"].empty()) {
    std::istringstream is(kv["ID_LIKE"]);
    is >> out.family;
  } else {
    out.family = out.distribution;
  }
  return out;
}

os_release_info read_os_release_from(const std::string &path) {
  std::ifstream ifs(path.c_str());
  if (!ifs.is_open()) return os_release_info();
  std::stringstream ss;
  ss << ifs.rdbuf();
  return parse_os_release(ss.str());
}

namespace os_version_filter {
filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("kernel_name", &filter_obj::get_kernel_name, "Kernel name")
      .add_string_var("nodename", &filter_obj::get_nodename, "Network node hostname")
      .add_string_var("kernel_release", &filter_obj::get_kernel_release, "Kernel release")
      .add_string_var("kernel_version", &filter_obj::get_kernel_version, "Kernel version")
      .add_string_var("machine", &filter_obj::get_machine, "Machine hardware name")
      .add_string_var("processor", &filter_obj::get_processor, "Processor / machine architecture")
      .add_string_var("os", &filter_obj::get_os, "Operating system (distribution pretty name, or kernel when unknown)")
      .add_string_var("distribution", &filter_obj::get_distribution, "Distribution id, e.g. 'ubuntu' (from /etc/os-release ID)")
      .add_string_var("distribution_name", &filter_obj::get_distribution_name, "Distribution name, e.g. 'Ubuntu' (from NAME)")
      .add_string_var("version", &filter_obj::get_version, "Distribution version, e.g. '22.04' (from VERSION_ID)")
      .add_string_var("family", &filter_obj::get_family, "Distribution family, e.g. 'debian' (from ID_LIKE/ID)");
}
}  // namespace os_version_filter
}  // namespace os_version

void os_version::check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef os_version_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${os} (kernel ${kernel_release})", "kernel_release", "", "");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  utsname name{};
  if (uname(&name) == -1) return nscapi::protobuf::functions::set_response_bad(*response, "Cannot get system name");

  const os_release_info osr = read_os_release_from("/etc/os-release");

  std::shared_ptr<os_version_filter::filter_obj> record(new os_version_filter::filter_obj());
  record->kernel_name = name.sysname;
  record->nodename = name.nodename;
  record->kernel_version = name.version;
  record->kernel_release = name.release;
  record->machine = name.machine;
  record->processor = name.machine;
  record->distribution = osr.distribution;
  record->distribution_name = osr.distribution_name;
  record->version = osr.version;
  record->family = osr.family;
  // Prefer the distribution pretty name; fall back to kernel identity so ${os}
  // is never empty even on hosts without /etc/os-release.
  record->os = !osr.pretty.empty() ? osr.pretty : (std::string(name.sysname) + " " + name.release);

  filter.match(record);

  filter_helper.post_process(filter);
}
