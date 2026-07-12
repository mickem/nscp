// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_group_members.hpp"

#include <boost/algorithm/string.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

namespace po = boost::program_options;

namespace group_members_filter {

bool is_expected(const filter_obj &m, const std::vector<std::string> &allow) {
  for (const std::string &a : allow) {
    if (boost::iequals(a, m.member) || boost::iequals(a, m.name)) return true;
  }
  return false;
}

using parsers::where::type_bool;
filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("group", &filter_obj::get_group, "The local group being checked")
      .add_string_var("member", &filter_obj::get_member, "Member as 'DOMAIN\\name'")
      .add_string_var("name", &filter_obj::get_name, "Member name component")
      .add_string_var("domain", &filter_obj::get_domain, "Member domain component (BUILTIN, machine, AD domain, ...)")
      .add_string_var("sid", &filter_obj::get_sid, "Member SID")
      .add_string_var("type", &filter_obj::get_type, "Member type: user, group, wellknown, alias, deleted, unknown");
  registry_.add_int_var("expected", type_bool, &filter_obj::get_expected, "True if the member is on the expected= allow-list (or no list was given)");
}
}  // namespace group_members_filter

namespace check_group_members_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<group_members_filter::filter> filter_helper(request, response, data);

  std::string group = "Administrators";
  std::vector<std::string> allow;

  group_members_filter::filter filter;
  // Default: CRITICAL on membership drift — a member that is not on the
  // expected= allow-list. When no allow-list is given every member is marked
  // expected, so the check simply lists the group's members (OK). empty_state=ok.
  filter_helper.add_options("", "expected = 0", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${member} (${type})", "${member}", "%(status): Group is empty",
                           "%(status): All %(count) member(s) are on the expected list.");
  filter_helper.set_default_perf_config("extra(count)");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("group", po::value<std::string>(&group), "Local group to inspect (default: Administrators)")
    ("expected", po::value<std::vector<std::string>>(&allow),
     "An allowed member (repeatable), matched against 'DOMAIN\\name' or the bare name. Any member NOT on this list is CRITICAL. "
     "When omitted, all members are listed instead.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<group_members_filter::filter_obj_ptr> members;
  std::string error;
  group_members_source::gather(group, members, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  // Mark drift: with no allow-list every member is expected; otherwise only
  // those present on the list.
  for (const group_members_filter::filter_obj_ptr &m : members) {
    m->expected = (allow.empty() || group_members_filter::is_expected(*m, allow)) ? 1 : 0;
  }

  parsers::where::constants::reset();
  for (const group_members_filter::filter_obj_ptr &m : members) filter.match(m);
  filter_helper.post_process(filter);
}

}  // namespace check_group_members_command
