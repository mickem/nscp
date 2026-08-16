// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_firewall_rules.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <str/xtos.hpp>

namespace po = boost::program_options;

namespace firewall_rules_filter {

std::string protocol_name(const long long protocol) {
  switch (protocol) {
    case protocol_tcp:
      return "tcp";
    case protocol_udp:
      return "udp";
    case protocol_icmpv4:
      return "icmpv4";
    case protocol_icmpv6:
      return "icmpv6";
    case protocol_any:
      return "any";
    default:
      return str::xtos(protocol);
  }
}

std::string profiles_name(const long long profile_mask) {
  if ((profile_mask & profile_all) == profile_all) return "all";
  std::vector<std::string> names;
  if ((profile_mask & profile_domain) != 0) names.push_back("domain");
  if ((profile_mask & profile_private) != 0) names.push_back("private");
  if ((profile_mask & profile_public) != 0) names.push_back("public");
  if (names.empty()) return "none";
  return boost::algorithm::join(names, ",");
}

bool is_unrestricted(const std::string &field) {
  const std::string value = boost::trim_copy(field);
  return value.empty() || value == "*";
}

void normalize_scope(std::string &field) {
  boost::trim(field);
  if (field.empty()) field = "*";
}

void classify(filter_obj &obj) {
  // The API leaves a scope field empty where the firewall UI shows "Any" (a
  // rule for every protocol has no ports to name, for instance). Normalise to
  // the "*" the same API uses elsewhere, so `local_ports = '*'` matches every
  // unrestricted rule rather than only some of them.
  normalize_scope(obj.local_ports);
  normalize_scope(obj.remote_ports);
  normalize_scope(obj.local_addresses);
  normalize_scope(obj.remote_addresses);

  obj.any_remote = is_unrestricted(obj.remote_addresses) ? 1 : 0;
  obj.any_port = is_unrestricted(obj.local_ports) ? 1 : 0;
  // Only inbound allows count. Outbound is unrestricted by default on Windows,
  // so treating a wide outbound allow as a finding would flag every machine.
  obj.any_any = (obj.enabled == 1 && obj.present == 1 && obj.direction == "in" && obj.action == "allow" && obj.any_remote == 1 && obj.any_port == 1) ? 1 : 0;
}

void apply_expectations(std::vector<filter_obj_ptr> &rules, const std::vector<std::string> &expected) {
  // Only real rules answer for an expectation: the loop below appends hole rows
  // to the same vector, and a duplicated expect= name must not find the hole
  // its first copy created and conclude "the rule exists but is disabled".
  const std::size_t real_rules = rules.size();
  std::vector<std::string> seen_names;
  for (const std::string &name : expected) {
    const auto already = std::find_if(seen_names.begin(), seen_names.end(), [&name](const std::string &s) { return boost::iequals(s, name); });
    if (already != seen_names.end()) continue;
    seen_names.push_back(name);

    bool satisfied = false;
    bool seen_disabled = false;
    for (std::size_t i = 0; i < real_rules; ++i) {
      const filter_obj_ptr &rule = rules[i];
      if (!boost::iequals(rule->name, name)) continue;
      rule->expected = 1;
      if (rule->enabled == 1) {
        satisfied = true;
      } else {
        seen_disabled = true;
      }
    }
    if (satisfied) continue;
    // Nothing enabled answers for this name: report the hole itself, so the
    // check fails on a rule that was removed as loudly as on one switched off.
    auto missing = std::make_shared<filter_obj>();
    missing->name = name;
    missing->present = 0;
    missing->expected = 1;
    missing->enabled = 0;
    missing->description = seen_disabled ? "the rule exists but is disabled" : "no rule with this name";
    rules.push_back(missing);
  }
}

std::string filter_obj::get_protocol() const { return protocol_name(protocol); }
std::string filter_obj::get_profiles() const { return profiles_name(profile_mask); }

std::string filter_obj::get_state() const {
  if (present == 0) return "not in effect: " + description;
  std::string state = direction + " " + action + " " + get_protocol();
  if (!is_unrestricted(local_ports)) state += " port " + local_ports;
  if (!is_unrestricted(remote_addresses)) state += " from " + remote_addresses;
  if (any_any == 1) state += " (unrestricted)";
  state += ", " + get_profiles();
  if (enabled == 0) state += ", disabled";
  return state;
}

using parsers::where::type_bool;
filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("name", &filter_obj::get_name, "Rule name as it appears in the firewall (localized on a localized Windows)")
      .add_string_var("description", &filter_obj::get_description, "Rule description")
      .add_string_var("group", &filter_obj::get_group, "Rule group, e.g. 'Remote Desktop' or '@FirewallAPI.dll,-28752'")
      .add_string_var("direction", &filter_obj::get_direction, "Direction the rule applies to: in or out")
      .add_string_var("action", &filter_obj::get_action, "What the rule does with matching traffic: allow or block")
      .add_string_var("protocol", &filter_obj::get_protocol, "Protocol: tcp, udp, icmpv4, icmpv6, any, or the raw protocol number")
      .add_string_var("profiles", &filter_obj::get_profiles, "Profiles the rule applies to: all, or a comma separated subset of domain, private and public")
      .add_string_var("local_ports", &filter_obj::get_local_ports, "Local ports the rule covers ('*' for any)")
      .add_string_var("remote_ports", &filter_obj::get_remote_ports, "Remote ports the rule covers ('*' for any)")
      .add_string_var("local_addresses", &filter_obj::get_local_addresses, "Local addresses the rule covers ('*' for any)")
      .add_string_var("remote_addresses", &filter_obj::get_remote_addresses, "Remote addresses the rule accepts traffic from ('*' for any)")
      .add_string_var("application", &filter_obj::get_application, "Program the rule is bound to (empty when it is not program specific)")
      .add_string_var("service", &filter_obj::get_service, "Service the rule is bound to (empty when it is not service specific)")
      .add_string_var("state", &filter_obj::get_state, "One line summary of what the rule does, or why an expected rule is not in effect");
  registry_.add_int_var("enabled", type_bool, &filter_obj::get_enabled, "True when the rule is switched on")
      .no_perf()
      .add_int_var("present", type_bool, &filter_obj::get_present,
                   "True for a real rule; false for an expect= name that no enabled rule satisfies (that is the default critical)")
      .no_perf()
      .add_int_var("expected", type_bool, &filter_obj::get_expected, "True when the rule matched one of the expect= names")
      .no_perf()
      .add_int_var("any_remote", type_bool, &filter_obj::get_any_remote, "True when the rule accepts traffic from any remote address")
      .no_perf()
      .add_int_var("any_port", type_bool, &filter_obj::get_any_port, "True when the rule covers any local port")
      .no_perf()
      .add_int_var("any_any", type_bool, &filter_obj::get_any_any,
                   "True for an enabled inbound allow rule that restricts neither the remote address nor the local port")
      .no_perf()
      .add_int_var("edge_traversal", type_bool, &filter_obj::get_edge_traversal, "True when the rule accepts traffic that has traversed a NAT device")
      .no_perf();
  // clang-format on
}
}  // namespace firewall_rules_filter

namespace check_firewall_rules_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<firewall_rules_filter::filter> filter_helper(request, response, data);

  std::vector<std::string> expected;

  firewall_rules_filter::filter filter;
  // Default: CRITICAL only for an expect= rule that is not in effect. Nothing
  // else alerts on its own - a machine has hundreds of rules and which of them
  // are too broad is a local judgement, so any_any is offered as a keyword
  // rather than imposed as a threshold.
  //
  // The top syntax lists the *problem* rules rather than every match: with
  // several hundred rules on a normal host, ${list} would render all of them.
  filter_helper.add_options("", "present = 0", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_list}", "${name}: ${state}", "${name}", "%(status): No firewall rules matched",
                           "%(status): %(count) rule(s) checked, all as expected");
  filter_helper.set_default_perf_config("extra(count)");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("expect", po::value<std::vector<std::string>>(&expected),
     "A rule that must exist and be enabled (repeatable), matched on the exact rule name, case insensitively. "
     "The check is CRITICAL when no enabled rule answers for the name - whether it was deleted or merely switched off. "
     "Rule names are localized, so take them from the machine you are checking.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  // An expectation is an assertion, not a filter candidate: a user filter such
  // as `enabled = 1` must not silently swallow the hole row that stands in for
  // a missing rule. Let hole rows through every filter expression.
  if (!expected.empty()) {
    for (std::string &expression : filter_helper.data.filter_string) {
      if (expression.empty() || expression == "none") continue;
      expression = "(" + expression + ") or present = 0";
    }
  }

  if (!filter_helper.build_filter(filter)) return;

  std::vector<firewall_rules_filter::filter_obj_ptr> rules;
  std::string error;
  firewall_rules_source::gather(rules, error);
  if (!error.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, error);
  }

  for (const firewall_rules_filter::filter_obj_ptr &rule : rules) firewall_rules_filter::classify(*rule);
  firewall_rules_filter::apply_expectations(rules, expected);

  parsers::where::constants::reset();
  for (const firewall_rules_filter::filter_obj_ptr &rule : rules) filter.match(rule);
  filter_helper.post_process(filter);
}

}  // namespace check_firewall_rules_command
