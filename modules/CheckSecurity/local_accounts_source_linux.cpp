// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux data source for check_local_accounts: the accounts in /etc/passwd (the
// local files only - NSS sources such as LDAP or sssd are deliberately not
// enumerated), joined with /etc/shadow for the password and expiry state.
//
// /etc/passwd is world-readable. /etc/shadow is not (root:shadow 0640 on
// Debian, root:root 0000 on RHEL): when it cannot be read, every account that
// defers to it ("x" in passwd) gets `shadow_error` and unknown flags instead of
// guessed ones. So does an account whose shadow entry is missing or unparseable,
// and a passwd line that cannot be parsed fails the check: nothing the system
// may honour as an account is dropped silently.
//
// A password hash never leaves this file: parse_shadow() keeps only the
// classification of the field, and parse errors report a line number.

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "account_files_linux.hpp"
#include "check_local_accounts.hpp"

namespace local_accounts_source {

namespace {

using account_files::for_each_entry;
using account_files::parse_number;
using account_files::split_fields;

const long long seconds_per_day = 86400;

// The characters of a DES-family crypt(3) hash.
bool is_crypt_alphabet(const std::string &text, const std::string::size_type from) {
  for (std::string::size_type i = from; i < text.size(); ++i) {
    const char c = text[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '/';
    if (!ok) return false;
  }
  return true;
}

// The numeric fields of a shadow line whose trailing fields are already padded
// out. Every one of them must be a number or empty, as glibc requires.
bool parse_shadow_numbers(const std::vector<std::string> &f, shadow_entry &e) {
  long long ignored = -1;
  return parse_number(f[2], e.last_change) && parse_number(f[3], ignored) && parse_number(f[4], e.max_days) && parse_number(f[5], ignored) &&
         parse_number(f[6], e.inactive) && parse_number(f[7], e.expire) && parse_number(f[8], ignored);
}

// True once PAM refuses the account whatever the credential: its expiry date
// has passed (pam_unix: today >= expire, an empty field meaning never), or its
// password expired longer ago than the inactivity period allows (pam_unix:
// days since the change >= max + inactive, both set, and a change recorded).
bool account_is_disabled(const shadow_entry &s, const long long today) {
  if (s.expire >= 0 && today >= s.expire) return true;
  if (s.last_change <= 0 || s.max_days < 0 || s.inactive < 0 || today < s.last_change) return false;
  if (s.max_days > (std::numeric_limits<long long>::max)() - s.inactive) return false;
  return today - s.last_change >= s.max_days + s.inactive;
}

// The shadow entry getspnam() would return for each name: the first one glibc
// can parse. A name whose only lines are malformed maps to one of those, so
// that the account is reported as unreadable rather than as absent.
std::map<std::string, const shadow_entry *> index_shadow(const std::vector<shadow_entry> &shadow) {
  std::map<std::string, const shadow_entry *> by_name;
  for (const shadow_entry &s : shadow) {
    const auto it = by_name.find(s.name);
    if (it == by_name.end()) {
      by_name.emplace(s.name, &s);
    } else if (it->second->malformed && !s.malformed) {
      it->second = &s;
    }
  }
  return by_name;
}

// The password and expiry state could not be read for this row: every flag
// stays unknown, and the check refuses the row unless asked not to.
void mark_unknown(local_accounts_filter::filter_obj &o, const std::string &why, const std::string &hint) {
  o.password_state = "unknown";
  o.shadow_error = why;
  o.shadow_hint = hint;
}

void apply_shadow(local_accounts_filter::filter_obj &o, const shadow_entry &s, const long long today) {
  o.password_state = s.state;
  o.disabled = account_is_disabled(s, today) ? 1 : 0;
  o.password_expires = (s.max_days >= 0 && s.max_days < 99999) ? 1 : 0;
  // 0 means "change at next logon", not 1970.
  if (s.last_change > 0) o.password_changed = s.last_change * seconds_per_day;
}

}  // namespace

std::string classify_password(const std::string &field) {
  if (field.empty()) return "empty";
  if (field[0] == '!') return "locked";
  // A crypt(3) hash: "$id$...", the 13-character DES form, bigcrypt (DES
  // continued past 13 characters) or BSDi extended DES ('_' and 19 characters).
  // Anything else ("*", "x", "NP", "LK", ...) cannot be matched by any password.
  if (field[0] == '$') return "set";
  if (field[0] == '_' && field.size() == 20 && is_crypt_alphabet(field, 1)) return "set";
  if (field.size() >= 13 && is_crypt_alphabet(field, 0)) return "set";
  return "none";
}

bool defers_to_shadow(const std::string &name, const std::string &password) {
  // pam_unix: "x", "##<name>" (is_pwd_shadowed) and the NIS+ marker "*NP*" all
  // send the lookup to getspnam().
  return password == "x" || password == "*NP*" || password == "##" + name;
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

std::vector<passwd_entry> parse_passwd(const std::string &content, std::vector<std::size_t> &skipped) {
  std::vector<passwd_entry> out;
  skipped.clear();
  for_each_entry(content, [&](const std::string &line, const std::size_t number) {
    const std::vector<std::string> f = split_fields(line);
    passwd_entry e;
    // glibc needs the name, password, uid and gid and takes the rest as empty:
    // "toor::0:0" is a passwordless root, not a broken line.
    if (f.size() < 4 || f[0].empty() || !parse_number(f[2], e.uid) || !parse_number(f[3], e.gid) || e.uid < 0 || e.gid < 0) {
      skipped.push_back(number);
      return;
    }
    e.name = f[0];
    e.password = f[1];
    if (f.size() > 5) e.home = f[5];
    // The shell is the rest of the line, further colons included.
    for (std::size_t i = 6; i < f.size(); ++i) e.shell += (i == 6 ? "" : ":") + f[i];
    out.push_back(e);
  });
  return out;
}

std::vector<shadow_entry> parse_shadow(const std::string &content, std::vector<std::size_t> &skipped) {
  std::vector<shadow_entry> out;
  skipped.clear();
  for_each_entry(content, [&](const std::string &line, const std::size_t number) {
    std::vector<std::string> f = split_fields(line);
    if (f[0].empty()) {
      skipped.push_back(number);
      return;
    }
    shadow_entry e;
    e.name = f[0];
    e.line = number;
    // The forms glibc returns: all nine fields, the eight without the reserved
    // one, or the old five-field form that stops after the maximum age.
    bool ok = f.size() == 5 || f.size() == 8 || f.size() == 9;
    if (ok) {
      const std::string state = classify_password(f[1]);
      f.resize(9);
      ok = parse_shadow_numbers(f, e);
      if (ok) e.state = state;
    }
    if (!ok) {
      // Kept, so that the account it names is reported as unreadable instead
      // of looking as if it had no entry - but with nothing taken from it.
      e = shadow_entry();
      e.name = f[0];
      e.line = number;
      e.malformed = true;
      e.state = "unknown";
      skipped.push_back(number);
    }
    out.push_back(e);
  });
  return out;
}

void build_accounts(const std::vector<passwd_entry> &passwd, const std::vector<shadow_entry> *shadow, const std::string &shadow_error, const long long now,
                    std::vector<local_accounts_filter::filter_obj_ptr> &out) {
  std::map<std::string, const shadow_entry *> by_name;
  if (shadow != nullptr) by_name = index_shadow(*shadow);
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
    out.push_back(o);

    if (!defers_to_shadow(p.name, p.password)) {
      // The hash (or its absence) lives in passwd itself, which is what PAM
      // then checks: no shadow needed. There is no ageing data without shadow.
      o->password_state = classify_password(p.password);
      o->disabled = 0;
      o->password_expires = 0;
    } else if (shadow == nullptr) {
      mark_unknown(*o, shadow_error, "run the agent with read access to it");
      continue;
    } else {
      // A shadowed account with no usable shadow entry has a password state
      // nobody read. Passing the thresholds on it would be the same false
      // negative as an unreadable file, so it is reported the same way.
      const auto it = by_name.find(p.name);
      if (it == by_name.end()) {
        mark_unknown(*o, "No entry for account '" + p.name + "' in /etc/shadow", "repair the file (see pwck)");
        continue;
      }
      if (it->second->malformed) {
        mark_unknown(*o, "Unparseable entry for account '" + p.name + "' in /etc/shadow (line " + std::to_string(it->second->line) + ")",
                     "repair the file (see pwck)");
        continue;
      }
      apply_shadow(*o, *it->second, today);
    }
    o->locked = o->password_state == "locked" ? 1 : 0;
    o->password_required = o->password_state == "empty" ? 0 : 1;
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
  std::vector<std::size_t> skipped;
  const std::vector<passwd_entry> passwd = parse_passwd(content, skipped);
  // A line glibc may still honour as an account, which this check cannot see:
  // answering for the rest of the file would hide it.
  if (!skipped.empty()) {
    error = account_files::unparsed_lines_error(passwd_path, skipped) + ": the accounts on them cannot be checked; repair the file (see pwck)";
    return;
  }

  std::string shadow_error;
  std::vector<shadow_entry> shadow;
  bool have_shadow = read_text_file(shadow_path, content, shadow_error);
  if (have_shadow) shadow = parse_shadow(content, skipped);
  build_accounts(passwd, have_shadow ? &shadow : nullptr, shadow_error, now, out);
}

}  // namespace local_accounts_source
