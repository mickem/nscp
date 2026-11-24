// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace local_accounts_filter {

// One local user account and its hygiene flags. Windows: Win32_UserAccount
// (LocalAccount=TRUE). Linux: an /etc/passwd entry joined with its /etc/shadow
// entry.
//
// The hygiene flags are optional because on Linux they come from /etc/shadow,
// which an unprivileged agent cannot read: they are then empty, render as
// 'unknown' and never satisfy a numeric comparison, and `shadow_error` says why
// (the check refuses to run on such rows unless asked to). Windows always fills
// them.
struct filter_obj {
  filter_obj() : disabled(0), locked(0), password_expires(0), password_required(0), rid(-1), now(0) {}

  std::string get_name() const { return name; }
  std::string get_sid() const { return sid; }
  boost::optional<long long> get_disabled() const { return disabled; }
  boost::optional<long long> get_enabled() const {
    if (!disabled) return boost::none;
    return disabled.value() ? 0 : 1;
  }
  boost::optional<long long> get_locked() const { return locked; }
  boost::optional<long long> get_password_expires() const { return password_expires; }
  boost::optional<long long> get_password_required() const { return password_required; }
  // Windows: the well-known relative ids, 500 = built-in Administrator and 501 =
  // built-in Guest. Linux: root (UID 0 named root) is the built-in
  // administrator; there is no built-in guest.
  long long get_is_builtin_admin() const { return (rid == 500 || (uid && uid.value() == 0 && name == "root")) ? 1 : 0; }
  long long get_is_builtin_guest() const { return rid == 501 ? 1 : 0; }
  // A UID 0 account other than root: a second superuser.
  long long get_is_extra_uid0() const { return (uid && uid.value() == 0 && name != "root") ? 1 : 0; }
  boost::optional<long long> get_uid() const { return uid; }
  boost::optional<long long> get_gid() const { return gid; }
  std::string get_home() const { return home; }
  std::string get_shell() const { return shell; }
  boost::optional<long long> get_interactive() const { return interactive; }
  std::string get_password_state() const { return password_state; }
  boost::optional<long long> get_password_changed() const { return password_changed; }
  boost::optional<long long> get_password_age() const {
    if (!password_changed) return boost::none;
    return now > password_changed.value() ? now - password_changed.value() : 0;
  }
  std::string show() const { return name; }

  std::string name;
  std::string sid;                               // Windows SID; Linux: the numeric UID
  boost::optional<long long> disabled;           // account disabled (Linux: expired)
  boost::optional<long long> locked;             // account locked out (Linux: password locked with '!')
  boost::optional<long long> password_expires;   // password is set to expire
  boost::optional<long long> password_required;  // a password is required for logon
  long long rid;                                 // SID relative id (last component); -1 if unknown / Linux

  // POSIX attributes (Linux); empty on Windows.
  boost::optional<long long> uid;
  boost::optional<long long> gid;
  std::string home;
  std::string shell;
  boost::optional<long long> interactive;       // shell is a real login shell (not nologin/false/...)
  std::string password_state;                   // set, empty, locked, none, unknown (Linux); empty on Windows
  boost::optional<long long> password_changed;  // epoch seconds of the last password change
  long long now;                                // epoch seconds the row was gathered (for password_age)
  std::string shadow_error;                     // why the shadow-derived flags are unknown; empty when known
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// Extract the relative id (last "-N" component) from a SID string, or -1 if it
// cannot be parsed. Exposed for reuse / clarity.
long long rid_from_sid(const std::string &sid);

}  // namespace local_accounts_filter

namespace local_accounts_source {
// Windows: WMI Win32_UserAccount where LocalAccount=TRUE. Linux: /etc/passwd
// joined with /etc/shadow. macOS: a stub that sets `error`.
void gather(std::vector<local_accounts_filter::filter_obj_ptr> &out, std::string &error);

// Linux readers (local_accounts_source_linux.cpp), split from gather() so the
// parsing can be tested against fixtures. None of them ever copies a password
// hash out of its input: only its classification (password_state) is kept, and
// error text names a line number, never a line.

// One /etc/passwd line: name:password:uid:gid:gecos:home:shell.
struct passwd_entry {
  std::string name;
  std::string password;  // "x" when the hash is in /etc/shadow
  long long uid = -1;
  long long gid = -1;
  std::string home;
  std::string shell;
};
// One /etc/shadow line: name:hash:lastchg:min:max:warn:inactive:expire:reserved.
// Day counts are days since the epoch; -1 when the field is empty.
struct shadow_entry {
  std::string name;
  std::string state;  // classify_password() of the hash
  long long last_change = -1;
  long long max_days = -1;
  long long expire = -1;
};

// Parse a whole file. Comments, blank lines and NIS compat lines (+/-) are
// skipped; a malformed line is skipped and counted in `skipped`.
std::vector<passwd_entry> parse_passwd(const std::string &content, std::size_t &skipped);
std::vector<shadow_entry> parse_shadow(const std::string &content, std::size_t &skipped);
// What a password field allows: "empty" (no password at all: passwordless
// logon), "locked" (starts with '!': usermod -L / passwd -l, or never set),
// "none" ('*' or any other value no password can match) or "set" (a crypt
// hash).
std::string classify_password(const std::string &field);
// True for a shell that cannot run an interactive session (nologin, false,
// true, sync, shutdown, halt). An empty shell is /bin/sh, so interactive.
bool is_interactive_shell(const std::string &shell);
// Build the rows. `shadow` is the content of /etc/shadow, or empty with
// `shadow_error` set when it could not be read. `now` is epoch seconds.
void build_accounts(const std::vector<passwd_entry> &passwd, const std::vector<shadow_entry> *shadow, const std::string &shadow_error, long long now,
                    std::vector<local_accounts_filter::filter_obj_ptr> &out);
// Read and build from the two files. `error` is set only when the passwd file
// cannot be read; an unreadable shadow file marks each affected row instead.
void gather_from(const std::string &passwd_path, const std::string &shadow_path, long long now, std::vector<local_accounts_filter::filter_obj_ptr> &out,
                 std::string &error);
// Read a whole file; false with `error` (path and strerror) when it cannot be
// read. Shared with check_group_members.
bool read_text_file(const std::string &path, std::string &content, std::string &error);
}  // namespace local_accounts_source

namespace check_local_accounts_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
