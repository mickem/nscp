// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace group_members_filter {

// One member of a local group (NetLocalGroupGetMembers). `expected` is set by
// the check from the `expected=` option (1 when the member is on the allow-list,
// or when no allow-list was given).
struct filter_obj {
  filter_obj() : expected(0) {}

  std::string get_group() const { return group; }
  std::string get_member() const { return member; }
  std::string get_name() const { return name; }
  std::string get_domain() const { return domain; }
  std::string get_sid() const { return sid; }
  std::string get_type() const { return type; }
  long long get_expected() const { return expected; }
  std::string show() const { return member; }

  std::string group;   // the group these members belong to
  std::string member;  // "DOMAIN\name" (or just "name")
  std::string name;    // name component
  std::string domain;  // domain component (BUILTIN, machine name, AD domain, ...)
  std::string sid;     // member SID as a string
  std::string type;    // user, group, wellknown, alias, deleted, unknown
  long long expected;  // 1 if allowed (or no allow-list given), 0 = drift
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// True if `member` (matched case-insensitively against its full "DOMAIN\name"
// or its bare name) is present in the allow-list. Exposed for clarity/reuse.
bool is_expected(const filter_obj &m, const std::vector<std::string> &allow);

}  // namespace group_members_filter

namespace group_members_source {
// Windows only (NetLocalGroupGetMembers). Sets `error` if the group does not
// exist or cannot be read. The Unix stub sets `error`.
void gather(const std::string &group, std::vector<group_members_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace group_members_source

namespace check_group_members_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
