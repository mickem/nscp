// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace local_accounts_filter {

// One local user account (Win32_UserAccount, LocalAccount=TRUE) and its hygiene
// flags.
struct filter_obj {
  filter_obj() : disabled(0), locked(0), password_expires(0), password_required(0), rid(-1) {}

  std::string get_name() const { return name; }
  std::string get_sid() const { return sid; }
  long long get_disabled() const { return disabled; }
  long long get_enabled() const { return disabled ? 0 : 1; }
  long long get_locked() const { return locked; }
  long long get_password_expires() const { return password_expires; }
  long long get_password_required() const { return password_required; }
  // Well-known relative ids: 500 = built-in Administrator, 501 = built-in Guest.
  long long get_is_builtin_admin() const { return rid == 500 ? 1 : 0; }
  long long get_is_builtin_guest() const { return rid == 501 ? 1 : 0; }
  std::string show() const { return name; }

  std::string name;
  std::string sid;
  long long disabled;           // account disabled
  long long locked;             // account locked out
  long long password_expires;   // password is set to expire
  long long password_required;  // a password is required for logon
  long long rid;                // SID relative id (last component); -1 if unknown
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
// Windows only (WMI Win32_UserAccount where LocalAccount=TRUE); the Unix stub
// sets `error`.
void gather(std::vector<local_accounts_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace local_accounts_source

namespace check_local_accounts_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
