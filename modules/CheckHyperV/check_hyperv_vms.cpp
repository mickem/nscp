// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hyperv_vms.hpp"

#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>
#include <win/com_helpers.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_hyperv_host.hpp"
#include "check_hyperv_internal.hpp"

namespace po = boost::program_options;

namespace check_hyperv {

using namespace check_hyperv_internal;

namespace {

const char *const kNamespace = "root\\virtualization\\v2";

// Property accessors that tolerate what differs between Hyper-V versions: a
// property that this version does not have (Get fails) or that is NULL reads
// as the default instead of failing the whole check.
long long int_or(const wmi_impl::row &row, const std::string &col, const long long def) {
  try {
    const boost::optional<long long> value = row.get_int_opt(col);
    return value ? value.value() : def;
  } catch (const wmi_impl::wmi_exception &) {
    return def;
  }
}

std::string string_or_empty(const wmi_impl::row &row, const std::string &col) {
  try {
    const std::string value = row.get_string(col);
    return value == "<NULL>" ? "" : value;
  } catch (const wmi_impl::wmi_exception &) {
    return "";
  }
}

// Run one query and hand every row to `fn`. The query object (and the WMI
// connection it holds) stays alive until the last row has been read, the way
// the other WMI checks keep it.
template <typename Fn>
void for_each_row(const std::string &wql, Fn fn) {
  wmi_impl::query wmi_query(wql, kNamespace, "", "");
  wmi_impl::row_enumerator rows = wmi_query.execute();
  while (rows.has_next()) fn(rows.get_next());
}

}  // namespace

std::string hyperv_unavailable(const wmi_impl::wmi_exception &e) {
  // Without the Hyper-V role the virtualization namespace does not exist. An
  // installed role whose management service is off keeps the namespace but
  // reports its classes as missing, and that has a different fix.
  if (e.get_code() == WBEM_E_INVALID_NAMESPACE) return "the Hyper-V role is not installed on this host (root\\virtualization\\v2 missing)";
  if (e.get_code() == WBEM_E_INVALID_CLASS || e.get_code() == WBEM_E_NOT_FOUND) {
    return "the Hyper-V management classes are missing - is the Hyper-V Virtual Machine Management service (vmms) running?";
  }
  return "";
}

std::string vms_hidden_from_caller(const std::size_t visible) {
  if (visible > 0) return "";
  long long counted = -1;
  try {
    counted = counted_vms(fetch_host_counters());
  } catch (const PDH::pdh_exception &) {
    // No counters to compare with: take the WMI answer as it is.
  }
  return hidden_vms_reason(visible, counted);
}

raw_rows fetch_vm_rows() {
  raw_rows rows;

  // SELECT * rather than a column list: a column this Hyper-V version lacks
  // (ReplicationMode arrived with 2012 R2) would fail the whole query, while a
  // missing property on a row just reads as its default above.
  for_each_row("SELECT * FROM Msvm_ComputerSystem", [&rows](const wmi_impl::row &row) {
    const std::string name = string_or_empty(row, "Name");
    if (!is_vm_guid(name)) return;  // the host's own row
    raw_vm vm;
    vm.id = to_lower(name);
    vm.name = string_or_empty(row, "ElementName");
    vm.enabled_state = int_or(row, "EnabledState", 0);
    vm.health_state = int_or(row, "HealthState", 0);
    vm.operational_status = parse_int_array(string_or_empty(row, "OperationalStatus"));
    vm.uptime_ms = int_or(row, "OnTimeInMilliseconds", 0);
    vm.last_state_change_epoch = str::format::parse_cim_datetime(string_or_empty(row, "TimeOfLastStateChange"));
    vm.process_id = int_or(row, "ProcessID", 0);
    vm.replication_mode = int_or(row, "ReplicationMode", 0);
    vm.replication_state = int_or(row, "ReplicationState", 0);
    vm.replication_health = int_or(row, "ReplicationHealth", 0);
    rows.vms.push_back(vm);
  });

  for_each_row("SELECT SystemName, EnabledState, OperationalStatus FROM Msvm_HeartbeatComponent", [&rows](const wmi_impl::row &row) {
    raw_heartbeat hb;
    hb.vm_id = to_lower(string_or_empty(row, "SystemName"));
    hb.enabled_state = int_or(row, "EnabledState", 0);
    hb.operational_status = parse_int_array(string_or_empty(row, "OperationalStatus"));
    rows.heartbeats.push_back(hb);
  });

  for_each_row("SELECT InstanceID, VirtualQuantity, Reservation, Limit, DynamicMemoryEnabled FROM Msvm_MemorySettingData", [&rows](const wmi_impl::row &row) {
    raw_memory_setting ms;
    ms.vm_id = settings_owner_guid(string_or_empty(row, "InstanceID"));
    if (ms.vm_id.empty()) return;  // a template or a snapshot's copy
    ms.startup_mb = int_or(row, "VirtualQuantity", 0);
    ms.minimum_mb = int_or(row, "Reservation", 0);
    ms.maximum_mb = int_or(row, "Limit", 0);
    ms.dynamic = int_or(row, "DynamicMemoryEnabled", 0) != 0;
    rows.memory_settings.push_back(ms);
  });

  for_each_row("SELECT SystemName, NumberOfBlocks, BlockSize FROM Msvm_Memory", [&rows](const wmi_impl::row &row) {
    raw_memory m;
    m.vm_id = to_lower(string_or_empty(row, "SystemName"));
    m.bytes = int_or(row, "NumberOfBlocks", 0) * int_or(row, "BlockSize", 0);
    rows.memory.push_back(m);
  });

  for_each_row("SELECT InstanceID, VirtualQuantity FROM Msvm_ProcessorSettingData", [&rows](const wmi_impl::row &row) {
    raw_processor_setting ps;
    ps.vm_id = settings_owner_guid(string_or_empty(row, "InstanceID"));
    if (ps.vm_id.empty()) return;
    ps.count = int_or(row, "VirtualQuantity", 0);
    rows.processor_settings.push_back(ps);
  });

  for_each_row("SELECT SystemName, LoadPercentage FROM Msvm_Processor", [&rows](const wmi_impl::row &row) {
    raw_processor p;
    p.vm_id = to_lower(string_or_empty(row, "SystemName"));
    p.load_percentage = int_or(row, "LoadPercentage", 0);
    rows.processors.push_back(p);
  });

  for_each_row("SELECT VirtualSystemIdentifier, VirtualSystemType, VirtualSystemSubType, Version, CreationTime FROM Msvm_VirtualSystemSettingData",
               [&rows](const wmi_impl::row &row) {
                 const std::string type = string_or_empty(row, "VirtualSystemType");
                 raw_settings s;
                 s.vm_id = to_lower(string_or_empty(row, "VirtualSystemIdentifier"));
                 if (type == "Microsoft:Hyper-V:System:Realized") {
                   s.snapshot = false;
                   s.sub_type = string_or_empty(row, "VirtualSystemSubType");
                   s.version = string_or_empty(row, "Version");
                 } else if (is_checkpoint_type(type)) {
                   s.snapshot = true;
                   s.creation_epoch = str::format::parse_cim_datetime(string_or_empty(row, "CreationTime"));
                 } else {
                   return;  // planned systems (an import or migration in flight), replica recovery points
                 }
                 rows.settings.push_back(s);
               });

  return rows;
}

namespace hyperv_vms_filter {

struct filter_obj {
  vm_record vm;
  explicit filter_obj(vm_record vm) : vm(std::move(vm)) {}

  std::string show() const { return vm.name + " (" + vm.state() + ")"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    using parsers::where::type_bool;
    using parsers::where::type_date;
    using parsers::where::type_float;
    using parsers::where::type_int;

    registry_.add_string_var("vm", [](auto obj) { return obj->vm.name; }, "Name of the virtual machine");
    registry_.add_string_var("id", [](auto obj) { return obj->vm.id; }, "GUID of the virtual machine");
    registry_.add_string_var("state", [](auto obj) { return obj->vm.state(); },
                             "Power state: running, off, saved, paused, starting, stopping, saving, pausing, resuming, fast_saved, fast_saving or "
                             "state_<n> for an unknown code");
    registry_.add_int_var("state_code", type_int, [](auto obj) { return obj->vm.state_code; }, "Raw EnabledState value behind `state`");
    registry_.add_string_var("health", [](auto obj) { return obj->vm.health(); }, "Health as the host sees it: ok, major_failure or critical_failure");
    registry_.add_string_var("operational_status", [](auto obj) { return obj->vm.operational_status(); },
                             "Operational status: ok, degraded, predictive_failure, stopped, in_service or dormant");
    registry_.add_string_var("operation", [](auto obj) { return obj->vm.operation(); },
                             "Long-running operation in progress (creating_snapshot, merging_disks, migrating, backing_up, ...) or none");
    registry_.add_string_var("heartbeat", [](auto obj) { return obj->vm.heartbeat; },
                             "What the guest's heartbeat integration service reports: ok, degraded, error, non_recoverable_error, no_contact, "
                             "lost_communication, dormant; disabled when the service is turned off in the VM settings, none when the VM has no "
                             "heartbeat component");
    registry_.add_int_var("uptime", type_int, [](auto obj) { return obj->vm.uptime_seconds; }, "Seconds since the VM was last started (0 when it is off)")
        .add_int_perf("s", "", "_uptime");
    registry_.add_int_var("last_state_change", type_date, [](auto obj) { return obj->vm.last_state_change_epoch; },
                          "When the VM last changed power state. Comparable to relative times, e.g. last_state_change > -1h.");
    registry_.add_int_var("pid", type_int, [](auto obj) { return obj->vm.process_id; }, "Process id of the VM's worker process (0 when it is off)");
    registry_.add_int_var("memory_assigned", type_int, [](auto obj) { return obj->vm.memory_assigned; },
                          "Memory currently assigned to the VM in bytes (0 when it is off)")
        .add_int_perf("B", "", "_memory_assigned");
    registry_.add_int_var("memory_startup", type_int, [](auto obj) { return obj->vm.memory_startup; }, "Configured startup memory in bytes")
        .add_int_perf("B", "", "_memory_startup");
    registry_.add_int_var("memory_minimum", type_int, [](auto obj) { return obj->vm.memory_minimum; },
                          "Configured minimum memory in bytes (dynamic memory only)")
        .add_int_perf("B", "", "_memory_minimum");
    registry_.add_int_var("memory_maximum", type_int, [](auto obj) { return obj->vm.memory_maximum; },
                          "Configured maximum memory in bytes (dynamic memory only)")
        .add_int_perf("B", "", "_memory_maximum");
    registry_.add_int_var("dynamic_memory", type_bool, [](auto obj) { return obj->vm.dynamic_memory ? 1LL : 0LL; },
                          "True when dynamic memory is enabled for the VM");
    registry_.add_int_var("vcpus", type_int, [](auto obj) { return obj->vm.vcpus; }, "Configured virtual processors").add_int_perf("", "", "_vcpus");
    registry_.add_numbers("cpu_load", type_float, [](auto obj) { return static_cast<long long>(obj->vm.cpu_load); },
                          [](auto obj) { return obj->vm.cpu_load; }, "Average load of the VM's virtual processors in % (0 when it is off)")
        .add_float_perf("%", "", "_cpu_load");
    registry_.add_int_var("generation", type_int, [](auto obj) { return obj->vm.generation; }, "VM generation (1 or 2)");
    registry_.add_string_var("version", [](auto obj) { return obj->vm.version; }, "Configuration version of the VM (e.g. 9.0)");
    registry_.add_int_var("snapshots", type_int, [](auto obj) { return obj->vm.snapshots; }, "Number of checkpoints (snapshots) the VM has")
        .add_int_perf("", "", "_snapshots");
    registry_.add_int_var("oldest_snapshot", type_date, [](auto obj) { return obj->vm.oldest_snapshot_epoch; },
                          "Creation time of the oldest checkpoint (0 / 'none' without checkpoints). Comparable to relative times, e.g. "
                          "oldest_snapshot < -7d.");
    registry_.add_string_var("replication_mode", [](auto obj) { return obj->vm.replication_mode_s(); },
                             "Hyper-V Replica role of the VM: none, primary, replica, test_replica or extended_replica");
    registry_.add_string_var("replication_state", [](auto obj) { return obj->vm.replication_state_s(); },
                             "Hyper-V Replica state: disabled, replicating, suspended, critical, resynchronizing, failover_in_progress, ...");
    registry_.add_string_var("replication_health", [](auto obj) { return obj->vm.replication_health_s(); },
                             "Hyper-V Replica health: not_applicable, ok, warning or critical");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace hyperv_vms_filter

void check_hyperv_vms(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using hyperv_vms_filter::filter;
  using hyperv_vms_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  // A running guest that stops answering the heartbeat is the classic hung
  // VM; a guest with the service turned off cannot answer, so it is left
  // alone. Health is what the host reports about the VM itself.
  filter_helper.add_options("state = 'running' and heartbeat != 'ok' and heartbeat != 'disabled'", "health != 'ok'", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_list}", "${vm}: ${state}, heartbeat ${heartbeat}, health ${health}", "${vm}", "No virtual machines found",
                           "%(status): all %(count) virtual machine(s) ok");
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("uptime");
  f.add_manual_perf("memory_assigned");
  f.add_manual_perf("cpu_load");

  const com_helper::mta_scope com;
  try {
    const std::vector<vm_record> records = build_records(fetch_vm_rows());
    const std::string hidden = vms_hidden_from_caller(records.size());
    if (!hidden.empty()) return nscapi::protobuf::functions::set_response_bad(*response, hidden);
    for (const vm_record &record : records) f.match(std::make_shared<filter_obj>(record));
  } catch (const wmi_impl::wmi_exception &e) {
    const std::string unavailable = hyperv_unavailable(e);
    if (!unavailable.empty()) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Hyper-V virtual machine information not available: " + unavailable);
    }
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to query Hyper-V virtual machines: " + e.reason());
  }

  filter_helper.post_process(f);
}

}  // namespace check_hyperv
