// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux data source for check_local_accounts: the accounts in /etc/passwd (the
// local files only - NSS sources such as LDAP or sssd are deliberately not
// enumerated), joined with /etc/shadow for the password and expiry state.
//
// /etc/passwd is world-readable. /etc/shadow is not (root:shadow 0640 on
// Debian, root:root 0000 on RHEL): when it cannot be read, every account that
// defers to it ("x" in passwd) gets `shadow_error` and unknown flags instead of
// guessed ones.
//
// A password hash never leaves this file: parse_shadow() keeps only the
// classification of the field, and parse errors report a line number.

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check_local_accounts.hpp"

namespace local_accounts_source {

namespace {

std::vector<std::string> split_fields(const std::string &line) {
  std::vector<std::string> out;
  std::string::size_type start = 0;
  for (;;) {
    const std::string::size_type colon = line.find(':', start);
    if (colon == std::string::npos) {
      out.push_back(line.substr(start));
      return out;
    }
    out.push_back(line.substr(start, colon - start));
    start = colon + 1;
  }
}

// A non-negative decimal number, or -1 for an empty field. False for anything
// else (so a corrupt line is skipped rather than half-read).
bool parse_number(const std::string &field, long long &value) {
  if (field.empty()) {
    value = -1;
    return true;
  }
  if (field.size() > 18) return false;
  long long v = 0;
  for (const char c : field) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
  }
  value = v;
  return true;
}

// Split into lines, dropping a trailing '\r', blank lines, comments and the
// NIS compat entries ("+", "+name", "-name") that refer to another source.
template <typename F>
void for_each_entry(const std::string &content, F f) {
  std::istringstream in(content);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#' || line[0] == '+' || line[0] == '-') continue;
    f(line);
  }
}

const long long seconds_per_day = 86400;

}  // namespace

std::string classify_password(const std::string &field) {
  if (field.empty()) return "empty";
  if (field[0] == '!') return "locked";
  // A crypt(3) hash: "$id$..." or the 13-character DES form. Anything else
  // ("*", "x", "NP", "LK", ...) cannot be matched by any password.
  if (field[0] == '$') return "set";
  if (field.size() == 13) {
    for (const char c : field) {
      const bool des = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '/';
      if (!des) return "none";
    }
    return "set";
  }
  return "none";
}

bool is_interactive_shell(const std::string &shell) {
  if (shell.empty()) return true;  // login(1) runs /bin/sh
  const std::string::size_type slash = shell.rfind('/');
  const std::string base = slash == std::string::npos ? shell : shell.substr(slash + 1);
  for (const char *blocked : {"nologin", "false", "true", "sync", "shutdown", "halt"}) {
    if (base == blocked) return false;
  }
  return true;
}

std::vector<passwd_entry> parse_passwd(const std::string &content, std::size_t &skipped) {
  std::vector<passwd_entry> out;
  skipped = 0;
  for_each_entry(content, [&](const std::string &line) {
    const std::vector<std::string> f = split_fields(line);
    passwd_entry e;
    if (f.size() != 7 || f[0].empty() || !parse_number(f[2], e.uid) || !parse_number(f[3], e.gid) || e.uid < 0 || e.gid < 0) {
      ++skipped;
      return;
    }
    e.name = f[0];
    e.password = f[1];
    e.home = f[5];
    e.shell = f[6];
    out.push_back(e);
  });
  return out;
}

std::vector<shadow_entry> parse_shadow(const std::string &content, std::size_t &skipped) {
  std::vector<shadow_entry> out;
  skipped = 0;
  for_each_entry(content, [&](const std::string &line) {
    const std::vector<std::string> f = split_fields(line);
    shadow_entry e;
    if (f.size() != 9 || f[0].empty() || !parse_number(f[2], e.last_change) || !parse_number(f[4], e.max_days) || !parse_number(f[7], e.expire)) {
      ++skipped;
      return;
    }
    e.name = f[0];
    e.state = classify_password(f[1]);
    out.push_back(e);
  });
  return out;
}

void build_accounts(const std::vector<passwd_entry> &passwd, const std::vector<shadow_entry> *shadow, const std::string &shadow_error, const long long now,
                    std::vector<local_accounts_filter::filter_obj_ptr> &out) {
  std::map<std::string, const shadow_entry *> by_name;
  if (shadow != nullptr) {
    for (const shadow_entry &s : *shadow) by_name.emplace(s.name, &s);  // first entry wins, as in getspnam()
  }
  const long long today = now / seconds_per_day;
  for (const passwd_entry &p : passwd) {
    auto o = std::make_shared<local_accounts_filter::filter_obj>();
    o->name = p.name;
    o->sid = std::to_string(p.uid);
    o->uid = p.uid;
    o->gid = p.gid;
    o->home = p.home;
    o->shell = p.shell;
    o->interactive = is_interactive_shell(p.shell) ? 1 : 0;
    o->now = now;
    o->disabled = boost::none;
    o->locked = boost::none;
    o->password_expires = boost::none;
    o->password_required = boost::none;

    if (p.password != "x") {
      // The hash (or its absence) lives in passwd itself, which is what PAM
      // then checks: no shadow needed. There is no ageing data without shadow.
      o->password_state = classify_password(p.password);
      o->disabled = 0;
      o->password_expires = 0;
    } else if (shadow == nullptr) {
      o->password_state = "unknown";
      o->shadow_error = shadow_error;
      out.push_back(o);
      continue;
    } else {
      const auto it = by_name.find(p.name);
      if (it == by_name.end()) {
        // "x" with no shadow entry: nothing to classify. Not a reason to fail
        // the whole check, but nothing to report either.
        o->password_state = "unknown";
        out.push_back(o);
        continue;
      }
      const shadow_entry &s = *it->second;
      o->password_state = s.state;
      // Expired accounts are refused by PAM whatever the credential (pam_unix:
      // today >= expire, with an empty field meaning never) - Linux's disabled.
      o->disabled = (s.expire >= 0 && today >= s.expire) ? 1 : 0;
      o->password_expires = (s.max_days >= 0 && s.max_days < 99999) ? 1 : 0;
      // 0 means "change at next logon", not 1970.
      if (s.last_change > 0) o->password_changed = s.last_change * seconds_per_day;
    }
    o->locked = o->password_state == "locked" ? 1 : 0;
    o->password_required = o->password_state == "empty" ? 0 : 1;
    out.push_back(o);
  }
}

bool read_text_file(const std::string &path, std::string &content, std::string &error) {
  content.clear();
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    error = "Cannot read " + path + ": " + std::strerror(errno);
    return false;
  }
  char buf[8192];
  for (;;) {
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n == 0) break;
    if (n < 0) {
      if (errno == EINTR) continue;
      const int e = errno;
      ::close(fd);
      content.clear();
      error = "Cannot read " + path + ": " + std::strerror(e);
      return false;
    }
    content.append(buf, static_cast<std::size_t>(n));
  }
  ::close(fd);
  return true;
}

void gather_from(const std::string &passwd_path, const std::string &shadow_path, const long long now, std::vector<local_accounts_filter::filter_obj_ptr> &out,
                 std::string &error) {
  std::string content;
  if (!read_text_file(passwd_path, content, error)) return;
  std::size_t skipped = 0;
  const std::vector<passwd_entry> passwd = parse_passwd(content, skipped);

  std::string shadow_error;
  std::vector<shadow_entry> shadow;
  bool have_shadow = read_text_file(shadow_path, content, shadow_error);
  if (have_shadow) shadow = parse_shadow(content, skipped);
  build_accounts(passwd, have_shadow ? &shadow : nullptr, shadow_error, now, out);
}

}  // namespace local_accounts_source
