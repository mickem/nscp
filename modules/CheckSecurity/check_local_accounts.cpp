// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_local_accounts.hpp"

#include <check/duration_keyword.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace po = boost::program_options;

namespace local_accounts_filter {

long long rid_from_sid(const std::string &sid) {
  const std::size_t dash = sid.rfind('-');
  if (dash == std::string::npos || dash + 1 >= sid.size()) return -1;
  try {
    return std::stoll(sid.substr(dash + 1));
  } catch (...) {
    return -1;
  }
}

using parsers::where::type_bool;
using parsers::where::type_int;
static const parsers::where::value_type type_custom_age = parsers::where::type_custom_int_1;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("name", &filter_obj::get_name, "Account name")
      .add_string_var("sid", &filter_obj::get_sid, "Account SID (Windows); the numeric UID on Linux, which has no SIDs")
      .add_string_var("home", &filter_obj::get_home, "Home directory (Linux; empty on Windows)")
      .add_string_var("shell", &filter_obj::get_shell, "Login shell (Linux; empty on Windows)")
      .add_string_var("password_state", &filter_obj::get_password_state,
                      "What the password field allows (Linux): set (a password hash), empty (no password: passwordless logon), locked (starts "
                      "with '!'), none ('*' or another value no password matches), unknown (/etc/shadow unreadable or no entry). Empty on Windows");
  // The hygiene flags are optional: on Linux they come from /etc/shadow and are
  // 'unknown' when the agent cannot read it, so they never trip a threshold on
  // a made-up value.
  registry_
      .add_optional_int_var("disabled", type_bool, [](auto obj) { return obj->get_disabled(); }, "unknown",
                            "True if the account is disabled (Linux: the account expiry date in /etc/shadow has passed)")
      .add_optional_int_var("enabled", type_bool, [](auto obj) { return obj->get_enabled(); }, "unknown",
                            "True if the account is enabled (convenience inverse of disabled)")
      .add_optional_int_var("locked", type_bool, [](auto obj) { return obj->get_locked(); }, "unknown",
                            "True if the account is locked out (Linux: the password is locked, its field starts with '!' - which also covers "
                            "accounts whose password was never set)")
      .add_optional_int_var("password_expires", type_bool, [](auto obj) { return obj->get_password_expires(); }, "unknown",
                            "True if the password is set to expire (Linux: a maximum password age below 99999 days)")
      .add_optional_int_var("password_required", type_bool, [](auto obj) { return obj->get_password_required(); }, "unknown",
                            "True if a password is required to log on (Linux: false only for an empty password field; '*' and '!' "
                            "accounts cannot log on with a password at all, so they count as required)")
      .add_optional_int_var("uid", type_int, [](auto obj) { return obj->get_uid(); }, "unknown", "Numeric user id (Linux; 'unknown' on Windows)")
      .no_perf()
      .add_optional_int_var("gid", type_int, [](auto obj) { return obj->get_gid(); }, "unknown", "Primary group id (Linux; 'unknown' on Windows)")
      .no_perf()
      .add_optional_int_var("interactive", type_bool, [](auto obj) { return obj->get_interactive(); }, "unknown",
                            "True if the login shell can run an interactive session, i.e. is not nologin, false, true, sync, shutdown or halt "
                            "(Linux; 'unknown' on Windows)")
      .add_optional_int_var("password_changed", parsers::where::type_date, [](auto obj) { return obj->get_password_changed(); }, "unknown",
                            "When the password was last changed, as epoch seconds (Linux, day resolution); compare with a relative date such as "
                            "password_changed < -90d. 'unknown' when not recorded, when a change is forced at next logon, or on Windows")
      .no_perf()
      .add_optional_int_var("password_age", type_custom_age, [](auto obj) { return obj->get_password_age(); }, "unknown",
                            "Seconds since the password was last changed (Linux); threshold with durations, e.g. password_age > 90d")
      .add_int_perf("s", "", "_password_age");
  registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<filter_obj_ptr>);
  registry_.add_int_var("is_builtin_admin", type_bool, &filter_obj::get_is_builtin_admin,
                        "True for the built-in administrator: RID 500 on Windows, root (UID 0) on Linux")
      .add_int_var("is_builtin_guest", type_bool, &filter_obj::get_is_builtin_guest, "True for the built-in Guest account (RID 501); always false on Linux")
      .add_int_var("is_extra_uid0", type_bool, &filter_obj::get_is_extra_uid0, "True for a UID 0 account other than root, i.e. a second superuser (Linux)");
  // clang-format on
}
}  // namespace local_accounts_filter

namespace check_local_accounts_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<local_accounts_filter::filter> filter_helper(request, response, data);

  local_accounts_filter::filter filter;
  // Default hygiene policy: WARNING if the built-in Guest account is enabled,
  // CRITICAL if an enabled account requires no password. Both are low-false-
  // positive on a hardened host; use the other keywords (password_expires,
  // is_builtin_admin, locked) to express a stricter policy. empty_state=ok.
  // On Linux there is no Guest account, and only an empty password field counts
  // as "no password required": the '*' / '!' system accounts cannot log on with
  // a password at all, so they stay OK.
  filter_helper.add_options("enabled = 1 and is_builtin_guest = 1", "enabled = 1 and password_required = 0", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${name} (enabled=${enabled}, pw_req=${password_required}, pw_exp=${password_expires}, locked=${locked})",
                           "${name}", "%(status): No local accounts found", "%(status): All %(count) local account(s) ok.");
  filter_helper.set_default_perf_config("extra(count)");

  bool allow_unreadable_shadow = false;
  // clang-format off
  filter_helper.get_desc().add_options()
    ("allow-unreadable-shadow", po::value<bool>(&allow_unreadable_shadow)->implicit_value(true)->default_value(false),
     "Linux: report the accounts even when /etc/shadow cannot be read (the agent is not root). The password and expiry keywords "
     "(enabled, disabled, locked, password_required, password_expires, password_*) are then 'unknown' and never match a threshold. "
     "Without it, an unreadable /etc/shadow makes the check UNKNOWN. No effect on Windows.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<local_accounts_filter::filter_obj_ptr> accounts;
  std::string error;
  local_accounts_source::gather(accounts, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }
  // An account whose password state could not be read would otherwise pass the
  // default thresholds silently: refuse unless the operator opted in.
  if (!allow_unreadable_shadow) {
    for (const local_accounts_filter::filter_obj_ptr &a : accounts) {
      if (!a->shadow_error.empty()) {
        return nscapi::protobuf::functions::set_response_bad(
            *response, a->shadow_error + " - password and expiry state is unknown; run the agent with read access to it or pass allow-unreadable-shadow");
      }
    }
  }

  parsers::where::constants::reset();
  for (const local_accounts_filter::filter_obj_ptr &a : accounts) filter.match(a);
  filter_helper.post_process(filter);
}

}  // namespace check_local_accounts_command
