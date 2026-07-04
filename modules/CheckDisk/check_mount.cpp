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

#include "check_mount.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <set>

#ifndef WIN32
#include <mntent.h>
#include <stdio.h>
#endif

namespace po = boost::program_options;

namespace check_mount_command {

struct filter_obj {
  std::string mount;
  std::string device;
  std::string fstype;
  std::string options;
  std::string issues;

  std::string get_mount() const { return mount; }
  std::string get_device() const { return device; }
  std::string get_fstype() const { return fstype; }
  std::string get_options() const { return options; }
  std::string get_issues() const { return issues; }
  long long get_has_issues() const { return issues.empty() ? 0 : 1; }
  std::string show() const { return "mount " + mount + " " + issues; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("mount", &filter_obj::get_mount, "Path of the mounted folder")
        .add_string_var("device", &filter_obj::get_device, "Device backing this mount")
        .add_string_var("fstype", &filter_obj::get_fstype, "Filesystem type of this mount")
        .add_string_var("options", &filter_obj::get_options, "Mount options")
        .add_string_var("issues", &filter_obj::get_issues, "Issues found (empty when the mount is as expected)");
    registry_.add_int_var("has_issues", &filter_obj::get_has_issues, "1 when any issue was found, else 0").no_perf();
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

namespace {
// Pseudo/virtual filesystems that are skipped when listing "all" mounts (they
// are always present and carry no useful mount semantics).
bool is_internal(const mount_point &m) {
  static const std::set<std::string> pseudo = {"autofs",   "proc",     "sysfs",    "tmpfs",    "devtmpfs",     "devpts",  "cgroup",
                                               "cgroup2",  "overlay",  "squashfs", "mqueue",   "hugetlbfs",    "debugfs", "tracefs",
                                               "securityfs", "pstore", "bpf",      "configfs", "binfmt_misc",  "fusectl", "ramfs",
                                               "nsfs",     "efivarfs", "fuse.gvfsd-fuse"};
  if (pseudo.count(m.fstype) > 0) return true;
  return boost::starts_with(m.mount, "/proc") || boost::starts_with(m.mount, "/sys") || boost::starts_with(m.mount, "/dev") ||
         boost::starts_with(m.mount, "/run");
}

std::set<std::string> to_set(const std::string &csv) {
  std::set<std::string> out;
  std::vector<std::string> parts;
  boost::split(parts, csv, boost::is_any_of(","));
  for (std::string p : parts) {
    boost::trim(p);
    if (!p.empty()) out.insert(p);
  }
  return out;
}

// Compute the issue string for one mount given the expected options/fstype.
std::string compute_issues(const mount_point &m, const std::string &expect_options, const std::string &expect_fstype, bool specific) {
  std::vector<std::string> issues;

  if (!expect_fstype.empty() && !boost::iequals(expect_fstype, m.fstype) && specific) {
    issues.push_back("expected fstype differs: " + expect_fstype + " != " + m.fstype);
  }

  if (!expect_options.empty()) {
    const std::set<std::string> want = to_set(expect_options);
    const std::set<std::string> have = to_set(m.options);
    std::vector<std::string> missing, exceeding;
    for (const std::string &w : want)
      if (have.count(w) == 0) missing.push_back(w);
    for (const std::string &h : have)
      if (want.count(h) == 0) exceeding.push_back(h);
    if (!missing.empty()) issues.push_back("missing options: " + boost::join(missing, ", "));
    if (!exceeding.empty()) issues.push_back("exceeding options: " + boost::join(exceeding, ", "));
  }

  return boost::join(issues, ", ");
}
}  // namespace

void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::vector<mount_point> &mounts) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::string mount_arg, options_arg, fstype_arg;

  filter_type filter;
  filter_helper.add_options("has_issues = 1", "issues like 'not mounted'", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_list}", "mount ${mount} ${issues}", "${mount}", "check_mount found nothing matching this filter",
                           "%(status): mounts are as expected");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("mount", po::value<std::string>(&mount_arg), "The mount point to check (omit to check all real mounts)")
    ("options", po::value<std::string>(&options_arg), "The mount options to expect (comma separated)")
    ("fstype", po::value<std::string>(&fstype_arg), "The filesystem type to expect")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  // A trailing slash on the requested mount point is stripped (but not on "/").
  if (mount_arg.size() > 1 && mount_arg.back() == '/') mount_arg.pop_back();
  const bool specific = !mount_arg.empty();

  bool matched_target = false;
  for (const mount_point &m : mounts) {
    if (specific) {
      if (m.mount != mount_arg) continue;
      matched_target = true;
    } else if (is_internal(m)) {
      continue;
    }
    // When listing all mounts, fstype acts as a selector; for a specific mount a
    // mismatch is reported as an issue instead (see compute_issues).
    if (!fstype_arg.empty() && !boost::iequals(fstype_arg, m.fstype) && !specific) continue;

    const std::shared_ptr<filter_obj> record(new filter_obj());
    record->mount = m.mount;
    record->device = m.device;
    record->fstype = m.fstype;
    record->options = m.options;
    record->issues = compute_issues(m, options_arg, fstype_arg, specific);
    filter.match(record);
  }

  // A specific mount that was never found is reported as "not mounted".
  if (specific && !matched_target) {
    const std::shared_ptr<filter_obj> record(new filter_obj());
    record->mount = mount_arg;
    record->issues = "not mounted";
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

std::vector<mount_point> read_mount_points() {
  std::vector<mount_point> result;
#ifndef WIN32
  FILE *fp = setmntent("/proc/mounts", "r");
  if (!fp) fp = setmntent("/etc/mtab", "r");
  if (!fp) return result;
  struct mntent ent;
  char buf[4096];
  while (getmntent_r(fp, &ent, buf, sizeof(buf)) != nullptr) {
    mount_point m;
    m.device = ent.mnt_fsname ? ent.mnt_fsname : "";
    m.mount = ent.mnt_dir ? ent.mnt_dir : "";
    m.fstype = ent.mnt_type ? ent.mnt_type : "";
    m.options = ent.mnt_opts ? ent.mnt_opts : "";
    result.push_back(m);
  }
  endmntent(fp);
#endif
  return result;
}

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
#ifdef WIN32
  nscapi::protobuf::functions::set_response_bad(*response, "check_mount is currently only implemented on Unix");
#else
  check_with(request, response, read_mount_points());
#endif
}

}  // namespace check_mount_command
