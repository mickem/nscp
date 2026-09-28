// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include "check_cluster.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace check_cluster {

std::string state_name(object_kind kind, long long state) {
  switch (kind) {
    case object_kind::group:
      switch (state) {
        case 0:
          return "online";
        case 1:
          return "offline";
        case 2:
          return "failed";
        case 3:
          return "partial_online";
        case 4:
          return "pending";
      }
      break;
    case object_kind::resource:
      switch (state) {
        case 0:
          return "inherited";
        case 1:
          return "initializing";
        case 2:
          return "online";
        case 3:
          return "offline";
        case 4:
          return "failed";
        case 128:
          return "pending";
        case 129:
          return "online_pending";
        case 130:
          return "offline_pending";
      }
      break;
    case object_kind::node:
      switch (state) {
        case 0:
          return "up";
        case 1:
          return "down";
        case 2:
          return "paused";
        case 3:
          return "joining";
      }
      break;
    case object_kind::network:
      switch (state) {
        case 0:
          return "unavailable";
        case 1:
          return "down";
        case 2:
          return "partitioned";
        case 3:
          return "up";
      }
      break;
  }
  return "unknown";
}

namespace {
struct filter_obj {
  record value;
  std::string state;
  filter_obj(record value, object_kind kind) : value(std::move(value)), state(state_name(kind, this->value.state_id)) {}
  std::string show() const { return value.name + ": " + state; }
};
struct filter_handler : parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> {
  filter_handler() {
    registry_.add_string_var(
                 "name", [](auto obj) { return obj->value.name; }, "Cluster object name")
        .add_string_var(
            "state", [](auto obj) { return obj->state; }, "Object state (lowercase; mappings differ by object kind)")
        .add_string_var(
            "owner", [](auto obj) { return obj->value.owner; }, "Current owner node (groups and resources)")
        .add_string_var(
            "group", [](auto obj) { return obj->value.group; }, "Containing group (resources)")
        .add_string_var("type", [](auto obj) { return obj->value.type; }, "Resource type (resources)");
    registry_.add_int_var("state_id", parsers::where::type_int, [](auto obj) { return obj->value.state_id; }, "Native state code for this object kind");
  }
};
using filter_type = modern_filter::modern_filters<filter_obj, filter_handler>;
}  // namespace

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, object_kind kind,
                const source &fetch) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> helper(request, response, data);
  filter_type filter;
  std::string warning, critical, detail = "${name}: ${state}", plural;
  switch (kind) {
    case object_kind::group:
      plural = "groups";
      warning = "state = 'pending' or state = 'partial_online'";
      critical = "state = 'failed' or state = 'offline'";
      detail += " (owner=${owner})";
      break;
    case object_kind::resource:
      plural = "resources";
      warning = "state = 'initializing' or state = 'pending' or state = 'online_pending' or state = 'offline_pending'";
      critical = "state = 'failed'";
      detail += " (group=${group}, owner=${owner}, type=${type})";
      break;
    case object_kind::node:
      plural = "nodes";
      warning = "state = 'paused' or state = 'joining'";
      critical = "state = 'down'";
      break;
    case object_kind::network:
      plural = "networks";
      warning = "state = 'partitioned'";
      critical = "state = 'down' or state = 'unavailable'";
      break;
  }
  helper.add_options(warning, critical, "", filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${status}: ${list}", detail, "${name}", "No cluster " + plural + " matched", "");
  std::string name;
  helper.get_desc().add_options()("name", boost::program_options::value<std::string>(&name),
                                  "Require one exact object name (case insensitive). Missing objects return UNKNOWN regardless of empty-state.");
  if (!helper.parse_options()) return;
  if (!helper.build_filter(filter)) return;

  try {
    // Acquire the complete snapshot before matching: a partial read must never
    // masquerade as a healthy cluster, even if some rows matched already.
    const auto rows = fetch(kind);
    bool found = false;
    for (const auto &row : rows) {
      if (!name.empty() && !boost::iequals(name, row.name)) continue;
      found = true;
      if (state_name(kind, row.state_id) == "unknown") {
        return nscapi::protobuf::functions::set_response_bad(*response, "Unknown cluster state " + std::to_string(row.state_id) + " for '" + row.name + "'");
      }
      filter.match(std::make_shared<filter_obj>(row, kind));
    }
    if (!name.empty() && !found) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Cluster object '" + name + "' not found");
    }
    helper.post_process(filter);
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to query cluster " + plural + ": " + e.what());
  }
}
}  // namespace check_cluster
