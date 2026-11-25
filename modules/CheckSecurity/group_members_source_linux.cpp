// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux data source for check_group_members: a group's members from the local
// files, /etc/group (the listed members) and /etc/passwd (the accounts whose
// primary group it is - membership that `getent group` does not show, which is
// why it is included). Both files are world-readable. NSS sources (LDAP, sssd)
// are not enumerated: a group that only exists in a directory is "not found".

#include <set>
#include <string>
#include <vector>

#include "account_files_linux.hpp"
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
  // The line number of every one of them, whatever it names. Membership is
  // granted by gid, so a damaged line under another name (or none) may still
  // be one the system reads as granting the group being checked.
  std::vector<std::size_t> unparsed;
};

// The /etc/group entries (name:password:gid:member,member,...). Comments,
// blank and NIS compat (+/-) lines are skipped; malformed lines are remembered
// by name. As in glibc, the member field may be missing altogether
// ("sudo:x:27" is a group with no listed members).
group_file parse_group(const std::string &content) {
  group_file out;
  account_files::for_each_entry(content, [&](const std::string &line, const std::size_t number) {
    const std::vector<std::string> f = account_files::split_fields(line);
    group_entry g;
    if (f.size() < 3 || f.size() > 4 || f[0].empty() || !account_files::parse_number(f[2], g.gid) || g.gid < 0) {
      out.malformed.insert(f[0]);
      out.unparsed.push_back(number);
      return;
    }
    g.name = f[0];
    if (f.size() == 4) {
      for (const std::string &member : account_files::split_fields(f[3], ',')) {
        if (!member.empty()) g.members.push_back(member);
      }
    }
    out.entries.push_back(g);
  });
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
  // Any other line that cannot be parsed may grant this group through its gid:
  // listing the members that could be read would hide the ones on it.
  if (!groups.unparsed.empty()) {
    error = account_files::unparsed_lines_error("/etc/group", groups.unparsed) + ": a line that cannot be parsed may grant group " + group +
            " through its gid, so its members cannot be listed; repair the file (see grpck)";
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
  std::vector<std::size_t> skipped;
  const std::vector<local_accounts_source::passwd_entry> passwd = local_accounts_source::parse_passwd(passwd_content, skipped);
  // A passwd line that cannot be parsed may be an account whose primary group
  // this is: listing the others would hide it.
  if (!skipped.empty()) {
    error = account_files::unparsed_lines_error("/etc/passwd", skipped) + ": primary-group members cannot be listed; repair the file (see pwck)";
    return;
  }

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
