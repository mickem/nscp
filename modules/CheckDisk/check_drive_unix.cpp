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

// Unix implementation of check_drivesize: enumerate mounts via getmntent and
// query space via statvfs. Field registration / derived math / filter
// scaffolding mirror check_drive_win.cpp so queries are portable.

#include "check_drive.hpp"

#include <mntent.h>
#include <sys/statvfs.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <list>
#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/helpers.hpp>
#include <set>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <utility>
#include <vector>

namespace po = boost::program_options;

namespace {

// Drive "type" codes. The string names match the Windows CheckDisk so the
// `type` filter field and `type in (...)` expressions are portable.
enum drive_type {
  dt_unknown = 0,
  dt_fixed = 1,
  dt_remote = 2,
  dt_removable = 3,
  dt_cdrom = 4,
  dt_ramdisk = 5,
  dt_total = 0x77,
};

std::string type_to_string(const long long type) {
  switch (type) {
    case dt_fixed:
      return "fixed";
    case dt_remote:
      return "remote";
    case dt_removable:
      return "removable";
    case dt_cdrom:
      return "cdrom";
    case dt_ramdisk:
      return "ramdisk";
    case dt_total:
      return "total";
    default:
      return "unknown";
  }
}

int do_convert_type(const std::string &keyword) {
  if (keyword == "fixed") return dt_fixed;
  if (keyword == "remote") return dt_remote;
  if (keyword == "removable") return dt_removable;
  if (keyword == "cdrom") return dt_cdrom;
  if (keyword == "ramdisk") return dt_ramdisk;
  if (keyword == "unknown") return dt_unknown;
  if (keyword == "total") return dt_total;
  return -1;
}

drive_type classify_fs(const std::string &fstype) {
  static const std::set<std::string> remote = {"nfs", "nfs4", "cifs", "smbfs", "smb3", "ncpfs", "afs", "9p", "fuse.sshfs", "glusterfs", "ceph", "beegfs"};
  static const std::set<std::string> ramdisk = {"tmpfs", "ramfs", "devtmpfs"};
  static const std::set<std::string> cdrom = {"iso9660", "udf"};
  if (remote.count(fstype)) return dt_remote;
  if (ramdisk.count(fstype)) return dt_ramdisk;
  if (cdrom.count(fstype)) return dt_cdrom;
  return dt_fixed;
}

bool is_pseudo_fs(const std::string &fstype) {
  static const std::set<std::string> pseudo = {
      "proc",       "sysfs",       "cgroup",     "cgroup2",         "devtmpfs",    "devpts",  "mqueue",      "hugetlbfs",
      "debugfs",    "tracefs",     "securityfs", "pstore",          "bpf",         "configfs", "fusectl",    "autofs",
      "binfmt_misc", "rpc_pipefs", "nsfs",       "efivarfs",        "ramfs",       "selinuxfs", "fuse.gvfsd-fuse", "fuse.portal",
      "overlay",    "tmpfs",       "sysvfs",
      // Read-only image mounts (snap packages): always 100% full by design.
      "squashfs",   "fuse.snapfuse"};
  return pseudo.count(fstype) > 0;
}

struct mount_entry {
  std::string device;
  std::string mountpoint;
  std::string fstype;
};

std::list<mount_entry> read_mounts() {
  std::list<mount_entry> ret;
  FILE *fp = setmntent("/proc/mounts", "r");
  if (!fp) fp = setmntent("/etc/mtab", "r");
  if (!fp) return ret;
  struct mntent ent;
  char buf[4096];
  while (getmntent_r(fp, &ent, buf, sizeof(buf)) != nullptr) {
    mount_entry e;
    e.device = ent.mnt_fsname ? ent.mnt_fsname : "";
    e.mountpoint = ent.mnt_dir ? ent.mnt_dir : "";
    e.fstype = ent.mnt_type ? ent.mnt_type : "";
    ret.push_back(e);
  }
  endmntent(fp);
  return ret;
}

struct drive_container {
  std::string device;
  std::string mountpoint;
  std::string fs;
  drive_type type;

  drive_container() : type(dt_unknown) {}
  drive_container(std::string device, std::string mountpoint, std::string fs, drive_type type)
      : device(std::move(device)), mountpoint(std::move(mountpoint)), fs(std::move(fs)), type(type) {}
};

struct filter_obj {
  drive_container drive;
  long long user_free;
  long long total_free;
  long long drive_size;
  long long inode_total;
  long long inode_free;
  bool has_size;
  bool writable;
  bool readable;

  explicit filter_obj(const drive_container &drive)
      : drive(drive), user_free(0), total_free(0), drive_size(0), inode_total(0), inode_free(0), has_size(false), writable(true), readable(true) {}

  std::string get_drive() const { return drive.mountpoint; }
  std::string get_letter() const { return ""; }
  std::string get_name() const { return drive.device; }
  std::string get_id() const { return drive.device; }
  std::string get_drive_or_id() const { return drive.mountpoint.empty() ? drive.device : drive.mountpoint; }
  std::string get_drive_or_name() const { return drive.mountpoint.empty() ? drive.device : drive.mountpoint; }
  std::string get_filesystem() const { return drive.fs; }
  std::string show() const { return drive.mountpoint.empty() ? drive.device : drive.mountpoint; }

  std::string get_flags() const {
    std::string ret;
    str::format::append_list(ret, "mounted");
    if (readable) str::format::append_list(ret, "readable");
    if (writable) str::format::append_list(ret, "writable");
    return ret;
  }

  void get_size(parsers::where::evaluation_context) {
    if (has_size) return;
    has_size = true;
    if (drive.type == dt_total) return;
    struct statvfs vfs;
    if (statvfs(drive.mountpoint.c_str(), &vfs) != 0) {
      readable = false;
      writable = false;
      user_free = total_free = drive_size = 0;
      return;
    }
    const unsigned long long bs = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    drive_size = static_cast<long long>(vfs.f_blocks * bs);
    total_free = static_cast<long long>(vfs.f_bfree * bs);
    user_free = static_cast<long long>(vfs.f_bavail * bs);
    inode_total = static_cast<long long>(vfs.f_files);
    inode_free = static_cast<long long>(vfs.f_ffree);
    writable = (vfs.f_flag & ST_RDONLY) == 0;
  }

  long long get_inodes_total(parsers::where::evaluation_context context) {
    get_size(context);
    return inode_total;
  }
  long long get_inodes_free(parsers::where::evaluation_context context) {
    get_size(context);
    return inode_free;
  }
  long long get_inodes_used(parsers::where::evaluation_context context) {
    get_size(context);
    return inode_total - inode_free;
  }
  long long get_inodes_free_pct(parsers::where::evaluation_context context) {
    get_size(context);
    return str::format::calc_pct_round(inode_free, inode_total);
  }
  long long get_inodes_used_pct(parsers::where::evaluation_context context) { return 100 - get_inodes_free_pct(context); }

  long long get_user_free(parsers::where::evaluation_context context) {
    get_size(context);
    return user_free;
  }
  long long get_total_free(parsers::where::evaluation_context context) {
    get_size(context);
    return total_free;
  }
  long long get_drive_size(parsers::where::evaluation_context context) {
    get_size(context);
    return drive_size;
  }
  long long get_total_used(parsers::where::evaluation_context context) {
    get_size(context);
    return drive_size - total_free;
  }
  long long get_user_used(parsers::where::evaluation_context context) {
    get_size(context);
    return drive_size - user_free;
  }
  long long get_user_free_pct(parsers::where::evaluation_context context) {
    get_size(context);
    return str::format::calc_pct_round(user_free, drive_size);
  }
  long long get_total_free_pct(parsers::where::evaluation_context context) {
    get_size(context);
    return str::format::calc_pct_round(total_free, drive_size);
  }
  long long get_user_used_pct(parsers::where::evaluation_context context) { return 100 - get_user_free_pct(context); }
  long long get_total_used_pct(parsers::where::evaluation_context context) { return 100 - get_total_free_pct(context); }
  long long get_is_mounted(parsers::where::evaluation_context) const { return 1; }

  std::string get_user_free_pct_human(parsers::where::evaluation_context context) {
    get_size(context);
    return str::format::format_pct(user_free, drive_size);
  }
  std::string get_total_free_pct_human(parsers::where::evaluation_context context) {
    get_size(context);
    return str::format::format_pct(total_free, drive_size);
  }
  std::string get_user_used_pct_human(parsers::where::evaluation_context context) {
    get_size(context);
    return str::format::format_pct(drive_size - user_free, drive_size);
  }
  std::string get_total_used_pct_human(parsers::where::evaluation_context context) {
    get_size(context);
    return str::format::format_pct(drive_size - total_free, drive_size);
  }
  std::string get_user_free_human(parsers::where::evaluation_context context) { return str::format::format_byte_units(get_user_free(context)); }
  std::string get_total_free_human(parsers::where::evaluation_context context) { return str::format::format_byte_units(get_total_free(context)); }
  std::string get_drive_size_human(parsers::where::evaluation_context context) { return str::format::format_byte_units(get_drive_size(context)); }
  std::string get_total_used_human(parsers::where::evaluation_context context) { return str::format::format_byte_units(get_total_used(context)); }
  std::string get_user_used_human(parsers::where::evaluation_context context) { return str::format::format_byte_units(get_user_used(context)); }

  long long get_type(parsers::where::evaluation_context) const { return drive.type; }
  std::string get_type_as_string(parsers::where::evaluation_context context) { return type_to_string(get_type(context)); }
  long long get_removable(parsers::where::evaluation_context) const { return drive.type == dt_removable ? 1 : 0; }
  long long get_hotplug(parsers::where::evaluation_context) const { return drive.type == dt_removable ? 1 : 0; }
  long long get_readable(parsers::where::evaluation_context) const { return readable ? 1 : 0; }
  long long get_writable(parsers::where::evaluation_context) const { return writable ? 1 : 0; }
  long long get_erasable(parsers::where::evaluation_context) const { return 0; }
  long long get_media_type(parsers::where::evaluation_context) const { return drive.type; }

  void append(const std::shared_ptr<filter_obj> &other) {
    user_free += other->user_free;
    total_free += other->total_free;
    drive_size += other->drive_size;
    inode_total += other->inode_total;
    inode_free += other->inode_free;
  }
  void make_total() {
    has_size = true;
    total_free = user_free = drive_size = 0;
    inode_total = inode_free = 0;
  }
};

parsers::where::node_type calculate_total_used(std::shared_ptr<filter_obj> object, parsers::where::evaluation_context context,
                                               parsers::where::node_type subject) {
  parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
  double number = value.get<1>();
  std::string unit = value.get<2>();
  if (unit == "%") {
    number = (static_cast<double>(object->get_drive_size(context)) * number) / 100.0;
  } else {
    number = str::format::decode_byte_units(number, unit);
  }
  return parsers::where::factory::create_int(static_cast<long long>(number));
}

parsers::where::node_type calculate_user_used(std::shared_ptr<filter_obj> object, parsers::where::evaluation_context context,
                                              parsers::where::node_type subject) {
  parsers::where::helpers::read_arg_type value = parsers::where::helpers::read_arguments(context, subject, "%");
  double number = value.get<1>();
  std::string unit = value.get<2>();
  if (unit == "%") {
    number = (static_cast<double>(object->get_user_free(context)) * number) / 100.0;
  } else {
    number = str::format::decode_byte_units(number, unit);
  }
  return parsers::where::factory::create_int(static_cast<long long>(number));
}

parsers::where::node_type convert_type(std::shared_ptr<filter_obj>, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  std::string keyword = subject->get_string_value(context);
  boost::to_lower(keyword);
  const int type = do_convert_type(keyword);
  if (type == -1) {
    context->error("Failed to convert type: " + keyword);
    return parsers::where::factory::create_false();
  }
  return parsers::where::factory::create_int(type);
}

long long get_zero() { return 0; }

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  static const parsers::where::value_type type_custom_total_used = parsers::where::type_custom_int_1;
  static const parsers::where::value_type type_custom_total_free = parsers::where::type_custom_int_2;
  static const parsers::where::value_type type_custom_user_used = parsers::where::type_custom_int_3;
  static const parsers::where::value_type type_custom_user_free = parsers::where::type_custom_int_4;
  static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_9;

  filter_obj_handler() {
    registry_.add_string_var("name", &filter_obj::get_name, "Descriptive name of drive (device)")
        .add_string_var("id", &filter_obj::get_id, "Drive or id of drive (device)")
        .add_string_var("drive", &filter_obj::get_drive, "Technical name of drive (mount point)")
        .add_string_var("letter", &filter_obj::get_letter, "Letter the drive is mounted on (always empty on Unix)")
        .add_string_var("flags", &filter_obj::get_flags, "String representation of flags")
        .add_string_var("drive_or_id", &filter_obj::get_drive_or_id, "Mount point if present if not use device")
        .add_string_var("drive_or_name", &filter_obj::get_drive_or_name, "Mount point if present if not use device")
        .add_string_var("filesystem", &filter_obj::get_filesystem, "Filesystem type as reported by the OS (e.g. ext4, xfs, btrfs, nfs)")
        .add_string_var("fs", &filter_obj::get_filesystem, "Shorthand alias for filesystem");
    // clang-format off
    registry_.add_int_legacy()
      ("free", type_custom_total_free, &filter_obj::get_total_free, "Shorthand for total_free (Number of free bytes)")
        .add_scaled_byte([] (auto, auto) { return get_zero(); }, &filter_obj::get_drive_size, "", " free")
        .add_percentage(&filter_obj::get_drive_size, "", " free %")
      ("total_free", type_custom_total_free, &filter_obj::get_total_free, "Number of free bytes")
        .add_scaled_byte([] (auto, auto) { return get_zero(); }, &filter_obj::get_drive_size, "", " free")
        .add_percentage(&filter_obj::get_drive_size, "", " free %")
      ("user_free", type_custom_user_free, &filter_obj::get_user_free, "Free space available to user (which runs NSClient++)")
        .add_scaled_byte([] (auto, auto) { return get_zero(); }, &filter_obj::get_drive_size, "", " user free")
        .add_percentage(&filter_obj::get_drive_size, "", " user free %")
      ("size", parsers::where::type_size, &filter_obj::get_drive_size, "Total size of drive")
      ("total_used", type_custom_total_used, &filter_obj::get_total_used, "Number of used bytes")
        .add_scaled_byte([] (auto, auto) { return get_zero(); }, &filter_obj::get_drive_size, "", " used")
        .add_percentage(&filter_obj::get_drive_size, "", " used %")
      ("used", type_custom_total_used, &filter_obj::get_total_used, "Number of used bytes")
        .add_scaled_byte([] (auto, auto) { return get_zero(); }, &filter_obj::get_drive_size, "", " used")
        .add_percentage(&filter_obj::get_drive_size, "", " used %")
      ("user_used", type_custom_user_used, &filter_obj::get_user_used, "Number of used bytes (related to user)")
        .add_scaled_byte([] (auto, auto) { return get_zero(); }, &filter_obj::get_drive_size, "", " user used")
        .add_percentage(&filter_obj::get_drive_size, "", " user used %")
      ("type", type_custom_type, &filter_obj::get_type, "Type of drive")
      ("free_pct", &filter_obj::get_total_free_pct, "Shorthand for total_free_pct (% free space)")
      ("total_free_pct", &filter_obj::get_total_free_pct, "% free space")
      ("user_free_pct", type_custom_user_free, &filter_obj::get_user_free_pct, "% free space available to user")
      ("used_pct", &filter_obj::get_total_used_pct, "Shorthand for total_used_pct (% used space)")
      ("total_used_pct", &filter_obj::get_total_used_pct, "% used space")
      ("user_used_pct", type_custom_user_used, &filter_obj::get_user_used_pct, "% used space available to user")
      ("inodes_total", parsers::where::type_int, &filter_obj::get_inodes_total, "Total number of inodes on the filesystem")
      ("inodes_free", parsers::where::type_int, &filter_obj::get_inodes_free, "Number of free inodes")
      ("inodes_used", parsers::where::type_int, &filter_obj::get_inodes_used, "Number of used inodes")
      ("inodes_free_pct", &filter_obj::get_inodes_free_pct, "% free inodes")
      ("inodes_used_pct", &filter_obj::get_inodes_used_pct, "% used inodes")
      ("mounted", parsers::where::type_int, &filter_obj::get_is_mounted, "Check if a drive is mounted")
      ("removable", &filter_obj::get_removable, "1 (true) if drive is removable")
      ("hotplug", &filter_obj::get_hotplug, "1 (true) if drive is hotplugable")
      ("readable", &filter_obj::get_readable, "1 (true) if drive is readable")
      ("writable", &filter_obj::get_writable, "1 (true) if drive is writable")
      ("erasable", &filter_obj::get_erasable, "1 (true) if drive is erasable")
      ("media_type", &filter_obj::get_media_type, "Get the media type")
    ;
    // clang-format on

    registry_.add_human_string_context("free", &filter_obj::get_total_free_human, "")
        .add_human_string_context("total_free", &filter_obj::get_total_free_human, "")
        .add_human_string_context("user_free", &filter_obj::get_user_free_human, "")
        .add_human_string_context("size", &filter_obj::get_drive_size_human, "")
        .add_human_string_context("total_used", &filter_obj::get_total_used_human, "")
        .add_human_string_context("used", &filter_obj::get_total_used_human, "")
        .add_human_string_context("user_used", &filter_obj::get_user_used_human, "")
        .add_human_string_context("type", &filter_obj::get_type_as_string, "")
        .add_human_string_context("free_pct", &filter_obj::get_total_free_pct_human, "")
        .add_human_string_context("total_free_pct", &filter_obj::get_total_free_pct_human, "")
        .add_human_string_context("user_free_pct", &filter_obj::get_user_free_pct_human, "")
        .add_human_string_context("used_pct", &filter_obj::get_total_used_pct_human, "")
        .add_human_string_context("total_used_pct", &filter_obj::get_total_used_pct_human, "")
        .add_human_string_context("user_used_pct", &filter_obj::get_user_used_pct_human, "");

    registry_.add_converter(type_custom_total_free, &calculate_total_used)
        .add_converter(type_custom_total_used, &calculate_total_used)
        .add_converter(type_custom_user_free, &calculate_user_used)
        .add_converter(type_custom_user_used, &calculate_user_used)
        .add_converter(type_custom_type, &convert_type);
  }
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

const mount_entry *find_containing_mount(const std::list<mount_entry> &mounts, const std::string &path) {
  const mount_entry *best = nullptr;
  std::size_t best_len = 0;
  for (const mount_entry &m : mounts) {
    if (path == m.mountpoint || (path.compare(0, m.mountpoint.size(), m.mountpoint) == 0 &&
                                 (m.mountpoint == "/" || path.size() == m.mountpoint.size() || path[m.mountpoint.size()] == '/'))) {
      if (m.mountpoint.size() >= best_len) {
        best = &m;
        best_len = m.mountpoint.size();
      }
    }
  }
  return best;
}

std::list<drive_container> find_drives(const std::vector<std::string> &drives, std::vector<std::string> &not_found) {
  std::list<drive_container> ret;
  const std::list<mount_entry> mounts = read_mounts();
  std::set<std::string> seen;

  for (const std::string &d : drives) {
    if (d == "*" || d == "all" || d == "all-drives" || d == "all-volumes" || d == "drives" || d == "volumes") {
      for (const mount_entry &m : mounts) {
        if (is_pseudo_fs(m.fstype)) continue;
        if (!seen.insert(m.mountpoint).second) continue;
        ret.emplace_back(m.device, m.mountpoint, m.fstype, classify_fs(m.fstype));
      }
    } else {
      struct statvfs probe;
      if (statvfs(d.c_str(), &probe) != 0) {
        not_found.push_back(d);
        continue;
      }
      const mount_entry *m = find_containing_mount(mounts, d);
      const std::string device = m ? m->device : d;
      const std::string fs = m ? m->fstype : "";
      if (!seen.insert(d).second) continue;
      ret.emplace_back(device, d, fs, classify_fs(fs));
    }
  }
  return ret;
}

void do_check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> drives, excludes;
  bool total = false;

  // clang-format off
  filter_type filter;
  filter_helper.add_options("used > 80%", "used > 90%", "mounted = 1", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status} ${problem_list}", "${drive_or_name}: ${used}/${size} used", "${drive_or_id}", "%(status): No drives found", "%(status) All %(count) drive(s) are ok");
  filter_helper.get_desc().add_options()
    ("drive", po::value<std::vector<std::string>>(&drives),
      "The drives to check.\nMultiple options can be used to check more than one mount or wildcards can be used to indicate multiple drives to check. Examples: drive=/, drive=/home, drive=*, drive=all-drives")
    ("exclude", po::value<std::vector<std::string>>(&excludes), "A list of drives (mount points) not to check")
    ("total", po::bool_switch(&total), "Include the total of all matching drives")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  if (drives.empty()) drives.emplace_back("*");
  if (!total) {
    auto it = std::find(drives.begin(), drives.end(), "total");
    if (it != drives.end()) {
      total = true;
      drives.erase(it);
    }
  }

  drive_container total_dc("total", "total", "", dt_total);
  std::shared_ptr<filter_obj> total_obj(new filter_obj(total_dc));
  if (total) total_obj->make_total();

  std::vector<std::string> not_found;
  const std::list<drive_container> resolved = find_drives(drives, not_found);
  if (!not_found.empty()) {
    std::string msg = "Drive";
    msg += not_found.size() == 1 ? " " : "s ";
    bool first = true;
    for (const std::string &d : not_found) {
      if (!first) msg += ", ";
      msg += d;
      first = false;
    }
    msg += not_found.size() == 1 ? " was not found" : " were not found";
    return nscapi::protobuf::functions::set_response_bad(*response, msg);
  }

  for (const drive_container &drive : resolved) {
    if (std::find(excludes.begin(), excludes.end(), drive.mountpoint) != excludes.end() ||
        std::find(excludes.begin(), excludes.end(), drive.device) != excludes.end())
      continue;
    std::shared_ptr<filter_obj> obj(new filter_obj(drive));
    filter.match(obj);
    if (filter.has_errors()) return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed: " + filter.get_errors());
    if (total) {
      obj->get_size(filter.context);
      total_obj->append(obj);
    }
  }
  if (total) {
    filter.match(total_obj);
    if (filter.has_errors()) return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed: " + filter.get_errors());
  }

  filter_helper.post_process(filter);
}

}  // namespace

namespace {
// `fetch-only` short-circuits the filter machinery and emits one line per
// mounted filesystem in `<<<df>>>` format: mountpoint fs total_kb used_kb
// avail_kb pct% mountpoint. Mirrors the Windows check_drive_win.cpp behaviour so
// the fetch-only contract is identical across platforms.
void do_fetch_only(PB::Commands::QueryResponseMessage::Response *response) {
  std::string body;
  std::vector<std::string> wanted = {"*"};
  std::vector<std::string> not_found;
  for (const drive_container &drive : find_drives(wanted, not_found)) {
    struct statvfs vfs;
    if (statvfs(drive.mountpoint.c_str(), &vfs) != 0) continue;
    const unsigned long long bs = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    const long long total_kb = static_cast<long long>(vfs.f_blocks * bs / 1024);
    const long long avail_kb = static_cast<long long>(vfs.f_bavail * bs / 1024);
    const long long free_kb = static_cast<long long>(vfs.f_bfree * bs / 1024);
    const long long used_kb = total_kb - free_kb;
    const long long pct = total_kb > 0 ? (used_kb * 100) / total_kb : 0;
    const std::string fs = drive.fs.empty() ? "unknown" : drive.fs;
    const std::string &mp = drive.mountpoint;
    if (!body.empty()) body += "\n";
    body += mp + " " + fs + " " + str::xtos(total_kb) + " " + str::xtos(used_kb) + " " + str::xtos(avail_kb) + " " + str::xtos(pct) + "% " + mp;
  }
  nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_drivesize", NSCAPI::query_return_codes::returnOK, body, "");
}
}  // namespace

void check_drive::check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  for (int i = 0; i < request.arguments_size(); i++) {
    const std::string &a = request.arguments(i);
    if (a == "fetch-only" || a == "--fetch-only") {
      do_fetch_only(response);
      return;
    }
  }
  do_check(request, response);
}

// Test seam: classify a filesystem type string into the drive-type keyword.
std::string checkdisk_unix_classify_fs(const std::string &fstype) { return type_to_string(classify_fs(fstype)); }
