// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hyperv_switches.hpp"

#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>
#include <win/com_helpers.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_hyperv_internal.hpp"
#include "check_hyperv_vms.hpp"

namespace check_hyperv {

using namespace check_hyperv_internal;

namespace {

const char *const kNamespace = "root\\virtualization\\v2";

template <typename Fn>
void for_each_row(const wmi_impl::wmi_service &service, const std::string &wql, Fn fn) {
  wmi_impl::query wmi_query(wql, kNamespace, "", "");
  wmi_query.instance = service;
  wmi_impl::row_enumerator rows = wmi_query.execute();
  while (rows.has_next()) fn(rows.get_next());
}

switch_rows fetch_switch_rows() {
  switch_rows rows;
  wmi_impl::wmi_service service(kNamespace, "", "");
  service.get();

  for_each_row(service, "SELECT Name, ElementName, EnabledState, HealthState, OperationalStatus FROM Msvm_VirtualEthernetSwitch",
               [&rows](const wmi_impl::row &row) {
                 raw_switch sw;
                 sw.id = to_lower(row.get_string_or_empty("Name"));
                 sw.name = row.get_string_or_empty("ElementName");
                 sw.enabled_state = row.get_int_or("EnabledState", 0);
                 sw.health_state = row.get_int_or("HealthState", 0);
                 sw.operational_status = parse_int_array(row.get_string_or_empty("OperationalStatus"));
                 rows.switches.push_back(sw);
               });

  // The switch's own port settings say what it is bound to: the physical
  // adapter (external) and/or the host's virtual adapter (internal). The same
  // class also holds every VM's connection, whose owner is a VM GUID and so
  // never joins a switch.
  for_each_row(service, "SELECT InstanceID, HostResource FROM Msvm_EthernetPortAllocationSettingData", [&rows](const wmi_impl::row &row) {
    raw_switch_port_setting ps;
    ps.switch_id = port_owner_guid(row.get_string_or_empty("InstanceID"));
    if (ps.switch_id.empty()) return;
    ps.kind = classify_host_resource(row.get_string_or_empty("HostResource"));
    if (ps.kind == port_kind::other) return;
    rows.port_settings.push_back(ps);
  });

  for_each_row(service, "SELECT SystemName FROM Msvm_EthernetSwitchPort", [&rows](const wmi_impl::row &row) {
    raw_switch_port port;
    port.switch_id = to_lower(row.get_string_or_empty("SystemName"));
    rows.ports.push_back(port);
  });
  return rows;
}

}  // namespace

namespace hyperv_switches_filter {

struct filter_obj {
  switch_record sw;
  explicit filter_obj(switch_record sw) : sw(std::move(sw)) {}

  std::string show() const { return sw.name + " (" + sw.type + ", " + sw.state() + ")"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    using parsers::where::type_bool;
    using parsers::where::type_int;

    registry_.add_string_var("switch", [](auto obj) { return obj->sw.name; }, "Name of the virtual switch");
    registry_.add_string_var("id", [](auto obj) { return obj->sw.id; }, "GUID of the virtual switch");
    registry_.add_string_var(
        "type", [](auto obj) { return obj->sw.type; },
        "Switch type: external (bound to a physical network adapter), internal (host and virtual machines only) or "
        "private (virtual machines only)");
    registry_.add_int_var(
        "management_os", type_bool, [](auto obj) { return obj->sw.management_os ? 1LL : 0LL; },
        "True when the host itself has a virtual network adapter on the switch (always for internal, optional for external)");
    registry_.add_string_var(
        "state", [](auto obj) { return obj->sw.state(); },
        "Switch state: enabled or not_applicable (both normal; Hyper-V reports most switches as not_applicable), disabled, "
        "enabled_but_offline, starting, shutting_down, ... or state_<n> for an unknown code");
    registry_.add_int_var("state_code", type_int, [](auto obj) { return obj->sw.state_code; }, "Raw EnabledState value behind `state`");
    registry_.add_string_var(
        "health", [](auto obj) { return obj->sw.health(); },
        "Health as the host sees it: ok, degraded, minor_failure, major_failure, critical_failure, non_recoverable_error or "
        "unknown");
    registry_.add_string_var(
        "operational_status", [](auto obj) { return obj->sw.operational_status(); },
        "Operational status: ok, degraded, predictive_failure, stopped, in_service or dormant");
    registry_
        .add_int_var(
            "ports", type_int, [](auto obj) { return obj->sw.ports; },
            "Ports on the switch: one per connected virtual machine network adapter plus the external and host ports")
        .add_int_perf("", "", "_ports");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace hyperv_switches_filter

void check_hyperv_switches(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using hyperv_switches_filter::filter;
  using hyperv_switches_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  // A switch has no power state of its own and Hyper-V reports most of them
  // as not_applicable (5) rather than enabled (2), so both are healthy.
  filter_helper.add_options("state not in ('enabled', 'not_applicable')", "health != 'ok'", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_list}", "${switch}: ${type}, ${state}, health ${health}", "${switch}", "No virtual switches found",
                           "%(status): all %(count) virtual switch(es) ok");
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("ports");

  const com_helper::mta_scope com;
  try {
    for (const switch_record &record : build_switch_records(fetch_switch_rows())) f.match(std::make_shared<filter_obj>(record));
  } catch (const wmi_impl::wmi_exception &e) {
    const std::string unavailable = hyperv_unavailable(e);
    if (!unavailable.empty()) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Hyper-V virtual switch information not available: " + unavailable);
    }
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to query Hyper-V virtual switches: " + e.reason());
  }

  filter_helper.post_process(f);
}

}  // namespace check_hyperv
