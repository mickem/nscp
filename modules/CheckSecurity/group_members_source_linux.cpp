// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux data source for check_group_members: a group's members from the local
// files, /etc/group (the listed members) and /etc/passwd (the accounts whose
// primary group it is - membership that `getent group` does not show, which is
// why it is included). Both files are world-readable. NSS sources (LDAP, sssd)
// are not enumerated: a group that only exists in a directory is "not found".

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check_group_members.hpp"
#include "check_local_accounts.hpp"

namespace group_members_source {

namespace {

struct group_entry {
  std::string name;
  long long gid = -1;
  std::vector<std::string> members;
};

struct group_file {
  std::vector<group_entry> entries;
  // The name field of every line that could not be parsed. A damaged line must
  // not make its group read as absent: the check would then answer for another
  // group (the next default candidate) or report "not found" for one that is
  // there.
  std::set<std::string> malformed;
};

// The /etc/group entries (name:password:gid:member,member,...). Comments,
// blank and NIS compat (+/-) lines are skipped; malformed lines are remembered
// by name.
group_file parse_group(const std::string &content) {
  group_file out;
  std::istringstream in(content);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#' || line[0] == '+' || line[0] == '-') continue;
    std::vector<std::string> f;
    std::string::size_type start = 0;
    for (;;) {
      const std::string::size_type colon = line.find(':', start);
      f.push_back(line.substr(start, colon == std::string::npos ? std::string::npos : colon - start));
      if (colon == std::string::npos) break;
      start = colon + 1;
    }
    if (f.size() != 4 || f[0].empty() || f[2].empty() || f[2].size() > 18) {
      out.malformed.insert(f[0]);
      continue;
    }
    group_entry g;
    g.name = f[0];
    g.gid = 0;
    bool numeric = true;
    for (const char c : f[2]) {
      if (c < '0' || c > '9') {
        numeric = false;
        break;
      }
      g.gid = g.gid * 10 + (c - '0');
    }
    if (!numeric) {
      out.malformed.insert(f[0]);
      continue;
    }
    std::string::size_type ms = 0;
    while (ms <= f[3].size() && !f[3].empty()) {
      const std::string::size_type comma = f[3].find(',', ms);
      const std::string m = f[3].substr(ms, comma == std::string::npos ? std::string::npos : comma - ms);
      if (!m.empty()) g.members.push_back(m);
      if (comma == std::string::npos) break;
      ms = comma + 1;
    }
    out.entries.push_back(g);
  }
  return out;
}

}  // namespace

std::string first_existing_group(const std::string &group_content, const std::vector<std::string> &candidates) {
  // A candidate whose line is malformed still counts as existing, so that
  // members_from() reports the damage instead of the next candidate quietly
  // standing in for it.
  const group_file groups = parse_group(group_content);
  for (const std::string &c : candidates) {
    if (groups.malformed.count(c) != 0) return c;
    for (const group_entry &g : groups.entries) {
      if (g.name == c) return c;
    }
  }
  return "";
}

void members_from(const std::string &group_content, const std::string &passwd_content, const std::string &group,
                  std::vector<group_members_filter::filter_obj_ptr> &out, std::string &error) {
  if (group.empty()) {
    error = "No default administrators group on this host (neither sudo nor wheel exists in /etc/group); pass group=<name>";
    return;
  }
  const group_file groups = parse_group(group_content);
  if (groups.malformed.count(group) != 0) {
    error = "Malformed entry for group " + group + " in /etc/group (expected name:password:gid:members); its members cannot be listed";
    return;
  }
  // A group can be spread over several lines, and the files backend of
  // initgroups(3) grants membership from every one of them, not only the first
  // that getgrnam() returns. It grants by gid, so a line with another name but
  // the same gid confers the same membership. All of them are read here.
  std::set<long long> gids;
  for (const group_entry &g : groups.entries) {
    if (g.name == group) gids.insert(g.gid);
  }
  if (gids.empty()) {
    error = "Local group not found: " + group;
    return;
  }
  std::size_t skipped = 0;
  const std::vector<local_accounts_source::passwd_entry> passwd = local_accounts_source::parse_passwd(passwd_content, skipped);

  std::set<std::string> seen;
  auto add = [&](const std::string &name) {
    if (!seen.insert(name).second) return;
    auto o = std::make_shared<group_members_filter::filter_obj>();
    o->group = group;
    o->member = name;
    o->name = name;
    o->case_sensitive = true;
    o->type = "unknown";
    for (const local_accounts_source::passwd_entry &p : passwd) {
      if (p.name == name) {
        o->type = "user";
        o->sid = std::to_string(p.uid);
        break;
      }
    }
    out.push_back(o);
  };
  for (const group_entry &g : groups.entries) {
    if (gids.count(g.gid) == 0) continue;
    for (const std::string &m : g.members) add(m);
  }
  for (const local_accounts_source::passwd_entry &p : passwd) {
    if (gids.count(p.gid) != 0) add(p.name);
  }
}

void gather_from(const std::string &group_path, const std::string &passwd_path, const std::string &group,
                 std::vector<group_members_filter::filter_obj_ptr> &out, std::string &error) {
  std::string group_content, passwd_content;
  if (!local_accounts_source::read_text_file(group_path, group_content, error)) return;
  if (!local_accounts_source::read_text_file(passwd_path, passwd_content, error)) return;
  members_from(group_content, passwd_content, group, out, error);
}

}  // namespace group_members_source
