// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_local_accounts.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

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
filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Account name").add_string_var("sid", &filter_obj::get_sid, "Account SID");
  registry_.add_int_var("disabled", type_bool, &filter_obj::get_disabled, "True if the account is disabled")
      .add_int_var("enabled", type_bool, &filter_obj::get_enabled, "True if the account is enabled")
      .add_int_var("locked", type_bool, &filter_obj::get_locked, "True if the account is locked out")
      .add_int_var("password_expires", type_bool, &filter_obj::get_password_expires, "True if the password is set to expire")
      .add_int_var("password_required", type_bool, &filter_obj::get_password_required, "True if a password is required to log on")
      .add_int_var("is_builtin_admin", type_bool, &filter_obj::get_is_builtin_admin, "True for the built-in Administrator account (RID 500)")
      .add_int_var("is_builtin_guest", type_bool, &filter_obj::get_is_builtin_guest, "True for the built-in Guest account (RID 501)");
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
  filter_helper.add_options("enabled = 1 and is_builtin_guest = 1", "enabled = 1 and password_required = 0", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${name} (enabled=${enabled}, pw_req=${password_required}, pw_exp=${password_expires}, locked=${locked})",
                           "${name}", "%(status): No local accounts found", "%(status): All %(count) local account(s) ok.");
  filter_helper.set_default_perf_config("extra(count)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<local_accounts_filter::filter_obj_ptr> accounts;
  std::string error;
  local_accounts_source::gather(accounts, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const local_accounts_filter::filter_obj_ptr &a : accounts) filter.match(a);
  filter_helper.post_process(filter);
}

}  // namespace check_local_accounts_command
