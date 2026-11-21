// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unix implementation of check_file_security: owner, group, mode bits and
// metadata of a path via stat(2), names via getpwuid_r/getgrgid_r, and the
// program behind a systemd unit. The POSIX ACL comes from read_acl(), which is
// the one piece that differs per kernel (check_file_security_linux.cpp).

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "check_file_security_unix.hpp"

namespace file_security_posix {

using file_security_filter::ace;

bool parse_acl(const std::vector<unsigned char> &blob, std::vector<acl_entry> &entries) {
  entries.clear();
  if (blob.empty()) return true;
  // A 4 byte little-endian version (2) followed by 8 byte entries:
  // u16 tag, u16 permissions, u32 id.
  if (blob.size() < 4 || (blob.size() - 4) % 8 != 0) return false;
  const auto u16 = [&blob](const std::size_t at) { return static_cast<unsigned>(blob[at]) | (static_cast<unsigned>(blob[at + 1]) << 8); };
  const auto u32 = [&blob](const std::size_t at) {
    return static_cast<unsigned long>(blob[at]) | (static_cast<unsigned long>(blob[at + 1]) << 8) | (static_cast<unsigned long>(blob[at + 2]) << 16) |
           (static_cast<unsigned long>(blob[at + 3]) << 24);
  };
  if (u32(0) != 2) return false;
  for (std::size_t at = 4; at < blob.size(); at += 8) entries.emplace_back(u16(at), u16(at + 2) & 7u, u32(at + 4));
  return true;
}

std::string mode_string(const unsigned long mode) {
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%04lo", mode & 07777ul);
  return buffer;
}

std::string rights_string(const unsigned perm) {
  std::string rights = "---";
  if ((perm & 4u) != 0) rights[0] = 'r';
  if ((perm & 2u) != 0) rights[1] = 'w';
  if ((perm & 1u) != 0) rights[2] = 'x';
  return rights;
}

namespace {

unsigned long rights_mask(const unsigned perm) {
  unsigned long mask = 0;
  if ((perm & 4u) != 0) mask |= file_security_filter::mask_read_data;
  if ((perm & 2u) != 0) mask |= file_security_filter::mask_write_data | file_security_filter::mask_append;
  if ((perm & 1u) != 0) mask |= file_security_filter::mask_execute;
  return mask;
}

std::string name_or_id(const name_lookup &lookup, const unsigned long id) {
  const std::string name = lookup ? lookup(id) : std::string();
  return name.empty() ? std::to_string(id) : name;
}

// `prefix` is "" for the access ACL and "default:" for a directory's default
// ACL. uid 0 and gid 0 are the system itself, the Unix counterpart of the
// SYSTEM/Administrators entries the Windows side never questions.
ace user_entry(const std::string &prefix, const unsigned long uid, const unsigned perm, const name_lookup &user_name) {
  ace entry(prefix + "user:" + name_or_id(user_name, uid), "uid:" + std::to_string(uid), rights_mask(perm), true);
  entry.rights = rights_string(perm);
  entry.trusted = uid == 0;
  return entry;
}
ace group_entry(const std::string &prefix, const unsigned long gid, const unsigned perm, const name_lookup &group_name) {
  ace entry(prefix + "group:" + name_or_id(group_name, gid), "gid:" + std::to_string(gid), rights_mask(perm), true);
  entry.rights = rights_string(perm);
  entry.trusted = gid == 0;
  return entry;
}
ace other_entry(const std::string &prefix, const unsigned perm) {
  ace entry(prefix + "other", "", rights_mask(perm), true);
  entry.rights = rights_string(perm);
  entry.world = true;
  return entry;
}

unsigned acl_mask(const std::vector<acl_entry> &acl) {
  for (const acl_entry &entry : acl) {
    if (entry.tag == tag_mask) return entry.perm;
  }
  return 7u;
}

}  // namespace

bool build_entries(const identity &id, const std::vector<acl_entry> &access, const std::vector<acl_entry> &defaults, const name_lookup &user_name,
                   const name_lookup &group_name, std::vector<ace> &out) {
  const unsigned owner_perm = static_cast<unsigned>((id.mode >> 6) & 7ul);
  unsigned group_perm = static_cast<unsigned>((id.mode >> 3) & 7ul);
  const unsigned other_perm = static_cast<unsigned>(id.mode & 7ul);

  bool extended = false;
  for (const acl_entry &entry : access) {
    if (entry.tag == tag_user || entry.tag == tag_group || entry.tag == tag_mask) extended = true;
  }
  const unsigned mask = acl_mask(access);
  if (extended) {
    // With an ACL the group mode bits hold the mask, not the owning group's
    // permissions: those are the ACL's own group entry, capped by the mask.
    for (const acl_entry &entry : access) {
      if (entry.tag == tag_group_obj) group_perm = entry.perm & mask;
    }
  }

  ace owner = user_entry("", id.uid, owner_perm, user_name);
  owner.owner = true;
  out.push_back(owner);
  if (extended) {
    for (const acl_entry &entry : access) {
      if (entry.tag == tag_user) out.push_back(user_entry("", entry.id, entry.perm & mask, user_name));
    }
  }
  out.push_back(group_entry("", id.gid, group_perm, group_name));
  if (extended) {
    for (const acl_entry &entry : access) {
      if (entry.tag == tag_group) out.push_back(group_entry("", entry.id, entry.perm & mask, group_name));
    }
  }
  out.push_back(other_entry("", other_perm));

  if (id.is_dir && !defaults.empty()) {
    // A default ACL grants nothing on the directory itself; it is what every
    // file created inside starts out with - the counterpart of an inherit-only
    // entry on a Windows folder, which is kept for the same reason. Its owner
    // and owning-group entries describe whoever creates the file (CREATOR
    // OWNER), so only the named entries and "other" say who else gets in.
    extended = true;
    const unsigned default_mask = acl_mask(defaults);
    for (const acl_entry &entry : defaults) {
      if (entry.tag == tag_user) out.push_back(user_entry("default:", entry.id, entry.perm & default_mask, user_name));
    }
    for (const acl_entry &entry : defaults) {
      if (entry.tag == tag_group) out.push_back(group_entry("default:", entry.id, entry.perm & default_mask, group_name));
    }
    for (const acl_entry &entry : defaults) {
      if (entry.tag == tag_other) out.push_back(other_entry("default:", entry.perm));
    }
  }
  return extended;
}

std::string exec_start_program(const std::string &value) {
  std::size_t at = 0;
  while (at < value.size() && (value[at] == ' ' || value[at] == '\t')) ++at;
  // systemd's executable prefixes; any combination may precede the path.
  while (at < value.size() && std::strchr("@-:+!", value[at]) != nullptr) ++at;
  if (at >= value.size()) return {};
  if (value[at] == '"' || value[at] == '\'') {
    const char quote = value[at];
    const std::size_t end = value.find(quote, at + 1);
    return end == std::string::npos ? value.substr(at + 1) : value.substr(at + 1, end - at - 1);
  }
  const std::size_t end = value.find_first_of(" \t", at);
  return end == std::string::npos ? value.substr(at) : value.substr(at, end - at);
}

namespace {

// Fold the ExecStart= assignments of one unit file (or drop-in) into `commands`.
// An empty assignment clears what came before, which is how a drop-in replaces
// the command of the unit it extends.
void read_exec_start(const std::string &file, std::vector<std::string> &commands) {
  std::ifstream in(file.c_str());
  std::string line, pending, section;
  while (std::getline(in, line)) {
    boost::trim_right_if(line, boost::is_any_of("\r"));
    if (!pending.empty()) {
      line = pending + " " + boost::trim_left_copy(line);
      pending.clear();
    }
    if (!line.empty() && line.back() == '\\') {
      pending = line.substr(0, line.size() - 1);
      continue;
    }
    boost::trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    if (line[0] == '[') {
      section = line;
      continue;
    }
    if (section != "[Service]") continue;
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos || boost::trim_copy(line.substr(0, equals)) != "ExecStart") continue;
    const std::string value = boost::trim_copy(line.substr(equals + 1));
    if (value.empty())
      commands.clear();
    else
      commands.push_back(value);
  }
}

bool is_regular(const std::string &path) {
  boost::system::error_code ec;
  return boost::filesystem::is_regular_file(path, ec) && !ec;
}

}  // namespace

std::string unit_program(const std::vector<std::string> &dirs, const std::string &service, std::string &error) {
  // A unit name is a file name, never a path: do not let one walk out of the
  // unit directories.
  if (service.empty() || service.find('/') != std::string::npos || service == "." || service == "..") {
    error = "Service not found: " + service;
    return {};
  }
  std::string unit = service;
  if (unit.find('.') == std::string::npos) unit += ".service";
  // An instance ("getty@tty1.service") is started from its template
  // ("getty@.service") unless it has a unit file of its own.
  std::vector<std::string> names = {unit};
  const std::size_t at = unit.find('@');
  const std::size_t dot = unit.rfind('.');
  if (at != std::string::npos && dot != std::string::npos && dot > at + 1) names.push_back(unit.substr(0, at + 1) + unit.substr(dot));

  std::string unit_file;
  for (const std::string &name : names) {
    for (const std::string &dir : dirs) {
      if (is_regular(dir + "/" + name)) {
        unit_file = dir + "/" + name;
        break;
      }
    }
    if (!unit_file.empty()) break;
  }
  if (unit_file.empty()) {
    error = "Service not found: " + service;
    return {};
  }

  std::vector<std::string> commands;
  read_exec_start(unit_file, commands);
  // Drop-ins apply in file name order whichever directory they live in; when
  // two directories hold the same file name the earlier directory wins. The
  // template's drop-ins come before the instance's own.
  for (auto name = names.rbegin(); name != names.rend(); ++name) {
    std::map<std::string, std::string> drop_ins;
    for (const std::string &dir : dirs) {
      boost::system::error_code ec;
      boost::filesystem::directory_iterator it(dir + "/" + *name + ".d", ec);
      const boost::filesystem::directory_iterator end;
      while (!ec && it != end) {
        const std::string file = it->path().filename().string();
        if (boost::ends_with(file, ".conf")) drop_ins.insert(std::make_pair(file, it->path().string()));
        it.increment(ec);
      }
    }
    for (const auto &drop_in : drop_ins) read_exec_start(drop_in.second, commands);
  }

  if (commands.empty()) {
    error = "Service " + service + " has no ExecStart command";
    return {};
  }
  const std::string program = exec_start_program(commands.front());
  if (program.empty()) error = "Could not determine the program of service " + service;
  return program;
}

}  // namespace file_security_posix

namespace file_security_source {

using file_security_filter::ace;
using file_security_filter::filter_obj;

namespace {

std::string user_name(const unsigned long uid) {
  std::vector<char> buffer(1024);
  for (;;) {
    struct passwd entry;
    struct passwd *found = nullptr;
    const int rc = ::getpwuid_r(static_cast<uid_t>(uid), &entry, buffer.data(), buffer.size(), &found);
    if (rc == ERANGE && buffer.size() < 1024 * 1024) {
      buffer.resize(buffer.size() * 4);
      continue;
    }
    return rc == 0 && found != nullptr && found->pw_name != nullptr ? std::string(found->pw_name) : std::string();
  }
}

std::string group_name(const unsigned long gid) {
  std::vector<char> buffer(1024);
  for (;;) {
    struct group entry;
    struct group *found = nullptr;
    const int rc = ::getgrgid_r(static_cast<gid_t>(gid), &entry, buffer.data(), buffer.size(), &found);
    // A group's member list is part of the record, so this one does outgrow
    // the first buffer on real hosts.
    if (rc == ERANGE && buffer.size() < 1024 * 1024) {
      buffer.resize(buffer.size() * 4);
      continue;
    }
    return rc == 0 && found != nullptr && found->gr_name != nullptr ? std::string(found->gr_name) : std::string();
  }
}

}  // namespace

std::string service_binary(const std::string &service, std::string &error) {
  // systemd's unit search path, highest precedence first.
  static const std::vector<std::string> dirs = {"/etc/systemd/system", "/run/systemd/system", "/usr/local/lib/systemd/system", "/usr/lib/systemd/system",
                                                "/lib/systemd/system"};
  return file_security_posix::unit_program(dirs, service, error);
}

void inspect(filter_obj &obj, std::vector<ace> &aces) {
  // stat, not lstat: a path the caller names is judged by what it leads to.
  struct stat st;
  if (::stat(obj.path.c_str(), &st) != 0) {
    const int code = errno;
    obj.exists = 0;
    obj.readable = 0;
    // "Not there" is reported through exists/state; anything else (a parent
    // directory the agent may not search, a stale mount) is worth spelling out.
    if (code != ENOENT && code != ENOTDIR) obj.error = std::strerror(code);
    return;
  }
  obj.exists = 1;
  obj.readable = 1;
  obj.is_dir = S_ISDIR(st.st_mode) ? 1 : 0;

  obj.owner_sid = std::to_string(static_cast<unsigned long>(st.st_uid));
  obj.owner = user_name(st.st_uid);
  if (obj.owner.empty()) obj.owner = obj.owner_sid;
  obj.group_sid = std::to_string(static_cast<unsigned long>(st.st_gid));
  obj.group = group_name(st.st_gid);
  if (obj.group.empty()) obj.group = obj.group_sid;

  obj.mode = file_security_posix::mode_string(st.st_mode);
  obj.setuid = (st.st_mode & S_ISUID) != 0 ? 1 : 0;
  obj.setgid = (st.st_mode & S_ISGID) != 0 ? 1 : 0;
  obj.sticky = (st.st_mode & S_ISVTX) != 0 ? 1 : 0;
  obj.size = static_cast<long long>(st.st_size);
  obj.written = static_cast<long long>(st.st_mtime);
  obj.access = static_cast<long long>(st.st_atime);
  obj.creation = static_cast<long long>(st.st_ctime);

  // An ACL that cannot be read (or decoded) leaves the mode bits as the only
  // evidence, and those can understate who has access - so the path is
  // reported as unreadable rather than judged on half the picture.
  std::vector<file_security_posix::acl_entry> access, defaults;
  std::vector<unsigned char> blob;
  std::string error;
  if (!file_security_posix::read_acl(obj.path, false, blob, error)) {
    obj.readable = 0;
    obj.error = error;
  } else if (!file_security_posix::parse_acl(blob, access)) {
    obj.readable = 0;
    obj.error = "Failed to read the POSIX ACL: unknown format";
  }
  if (obj.is_dir != 0 && obj.readable != 0) {
    if (!file_security_posix::read_acl(obj.path, true, blob, error)) {
      obj.readable = 0;
      obj.error = error;
    } else if (!file_security_posix::parse_acl(blob, defaults)) {
      obj.readable = 0;
      obj.error = "Failed to read the POSIX ACL: unknown format";
    }
  }

  file_security_posix::identity id;
  id.uid = st.st_uid;
  id.gid = st.st_gid;
  id.mode = st.st_mode;
  id.is_dir = obj.is_dir != 0;
  obj.has_acl = file_security_posix::build_entries(id, access, defaults, user_name, group_name, aces) ? 1 : 0;
}

}  // namespace file_security_source
