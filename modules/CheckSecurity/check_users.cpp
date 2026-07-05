// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_users.hpp"

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace users_filter {

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("user", &filter_obj::get_user, "The account name of the logged-on user")
      .add_string_var("session_state", &filter_obj::get_session_state, "Session state (active, disconnected, ...)")
      .add_string_var("session_type", &filter_obj::get_session_type, "Session type (console, rdp, remote, ...)")
      .add_string_var("client", &filter_obj::get_client, "Client name or remote host (may be empty)");
}

}  // namespace users_filter

namespace check_users_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<users_filter::filter> filter_helper(request, response, data);

  users_filter::filter filter;
  // No default alert: this check is a count/inventory tool. `count` is a built-in
  // summary variable, so users write e.g. "crit=count > 10".
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${count} user(s) logged on: ${list}", "${user}", "${user}", "No users logged on",
                           "${status}: ${count} user(s) logged on");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<users_filter::filter_obj_ptr> sessions;
  std::string error;
  users_source::gather(sessions, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  parsers::where::constants::reset();
  for (const users_filter::filter_obj_ptr &s : sessions) filter.match(s);
  filter_helper.post_process(filter);
}

}  // namespace check_users_command
