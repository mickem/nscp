// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

// Platform-neutral half of the Hyper-V checks: the value tables of the
// root\virtualization\v2 classes, the parsing of what WMI hands back as text,
// and the joins that turn the per-class rows into one record per virtual
// machine. Nothing here touches WMI or PDH, so the unit test compiles it on
// any platform.
namespace check_hyperv {
namespace check_hyperv_internal {

// ---------------------------------------------------------------------------
// Value tables
// ---------------------------------------------------------------------------

// Msvm_ComputerSystem.EnabledState. The low values are the CIM standard, the
// 327xx ones are the Hyper-V transitions (saving, pausing, ...).
inline std::string vm_state_name(const long long state) {
  switch (state) {
    case 0:
      return "unknown";
    case 1:
      return "other";
    case 2:
      return "running";
    case 3:
      return "off";
    case 4:
      return "stopping";
    case 5:
      return "not_applicable";
    case 6:
      return "saved";
    case 7:
      return "in_test";
    case 8:
      return "deferred";
    case 9:
      return "paused";
    case 10:
      return "starting";
    case 11:
      return "reset";
    case 32773:
      return "saving";
    case 32776:
      return "pausing";
    case 32777:
      return "resuming";
    case 32779:
      return "fast_saved";
    case 32780:
      return "fast_saving";
    default:
      return "state_" + std::to_string(state);
  }
}

// CIM HealthState, as Msvm_ComputerSystem and Msvm_VirtualEthernetSwitch
// carry it. Hyper-V itself only sets 5, 20 and 25; the rest of the CIM table
// is decoded so a value from a newer provider still reads as a word.
inline std::string vm_health_name(const long long health) {
  switch (health) {
    case 0:
      return "unknown";
    case 5:
      return "ok";
    case 10:
      return "degraded";
    case 15:
      return "minor_failure";
    case 20:
      return "major_failure";
    case 25:
      return "critical_failure";
    case 30:
      return "non_recoverable_error";
    default:
      return "health_" + std::to_string(health);
  }
}

// First element of Msvm_ComputerSystem.OperationalStatus.
inline std::string vm_operational_status_name(const long long status) {
  switch (status) {
    case 2:
      return "ok";
    case 3:
      return "degraded";
    case 5:
      return "predictive_failure";
    case 10:
      return "stopped";
    case 11:
      return "in_service";
    case 15:
      return "dormant";
    default:
      return "status_" + std::to_string(status);
  }
}

// Second element of Msvm_ComputerSystem.OperationalStatus: the long-running
// operation the VM is in the middle of, if any.
inline std::string vm_operation_name(const long long status) {
  switch (status) {
    case 32768:
      return "creating_snapshot";
    case 32769:
      return "applying_snapshot";
    case 32770:
      return "deleting_snapshot";
    case 32771:
      return "waiting_to_start";
    case 32772:
      return "merging_disks";
    case 32773:
      return "exporting";
    case 32774:
      return "migrating";
    case 32776:
      return "backing_up";
    case 32777:
      return "modifying";
    case 32778:
      return "storage_migration";
    default:
      return "operation_" + std::to_string(status);
  }
}

// First element of Msvm_HeartbeatComponent.OperationalStatus, i.e. what the
// guest's heartbeat integration service reports.
inline std::string heartbeat_name(const long long status) {
  switch (status) {
    case 2:
      return "ok";
    case 3:
      return "degraded";
    case 6:
      return "error";
    case 7:
      return "non_recoverable_error";
    case 12:
      return "no_contact";
    case 13:
      return "lost_communication";
    case 15:
      return "dormant";
    default:
      return "heartbeat_" + std::to_string(status);
  }
}

// Msvm_ComputerSystem.ReplicationHealth.
inline std::string replication_health_name(const long long health) {
  switch (health) {
    case 0:
      return "not_applicable";
    case 1:
      return "ok";
    case 2:
      return "warning";
    case 3:
      return "critical";
    default:
      return "health_" + std::to_string(health);
  }
}

// Msvm_ComputerSystem.ReplicationState.
inline std::string replication_state_name(const long long state) {
  switch (state) {
    case 0:
      return "disabled";
    case 1:
      return "ready_for_initial_replication";
    case 2:
      return "waiting_to_complete_initial_replication";
    case 3:
      return "replicating";
    case 4:
      return "synced_replication_complete";
    case 5:
      return "recovered";
    case 6:
      return "committed";
    case 7:
      return "suspended";
    case 8:
      return "critical";
    case 9:
      return "waiting_to_start_resynchronization";
    case 10:
      return "resynchronizing";
    case 11:
      return "resynchronization_suspended";
    case 12:
      return "failover_in_progress";
    case 13:
      return "failback_in_progress";
    case 14:
      return "failback_complete";
    default:
      return "state_" + std::to_string(state);
  }
}

// Msvm_ComputerSystem.ReplicationMode.
inline std::string replication_mode_name(const long long mode) {
  switch (mode) {
    case 0:
      return "none";
    case 1:
      return "primary";
    case 2:
      return "replica";
    case 3:
      return "test_replica";
    case 4:
      return "extended_replica";
    default:
      return "mode_" + std::to_string(mode);
  }
}

// ---------------------------------------------------------------------------
// Parsing what WMI hands back as text
// ---------------------------------------------------------------------------

// The WMI row accessor renders an array property as "[2, 32768]"; pull the
// numbers back out. Anything that is not a number is skipped, so a malformed
// or empty array yields an empty vector rather than a bogus element.
inline std::vector<long long> parse_int_array(const std::string &text) {
  std::vector<long long> out;
  std::string token;
  const auto flush = [&]() {
    if (token.empty()) return;
    char *end = nullptr;
    const long long value = std::strtoll(token.c_str(), &end, 10);
    if (end != nullptr && *end == '\0') out.push_back(value);
    token.clear();
  };
  for (const char c : text) {
    if ((c >= '0' && c <= '9') || c == '-') {
      token.push_back(c);
    } else {
      flush();
    }
  }
  flush();
  return out;
}

// A Msvm_ComputerSystem row is a virtual machine when its Name is the VM's
// GUID; the one row for the host itself carries the computer name instead.
// Keying on the shape of the name rather than on Caption keeps the check
// working on localised Windows, where Caption reads "Virtueller Computer".
inline bool is_vm_guid(const std::string &name) {
  if (name.size() != 36) return false;
  for (std::size_t i = 0; i < name.size(); ++i) {
    const char c = name[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (c != '-') return false;
    } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      return false;
    }
  }
  return true;
}

inline std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// The VM a resource-setting row (Msvm_MemorySettingData, Msvm_ProcessorSettingData)
// belongs to. Its InstanceID is "Microsoft:<owning settings id>\<resource guid>",
// where the owning settings id is the VM GUID for the live configuration; the
// template rows ("Microsoft:Definition\...") and the rows of a snapshot yield
// an empty string, so they never join. A snapshot's settings id is either not
// a VM GUID ("<vm guid>:<snapshot guid>") or the VM GUID followed by a second
// path segment ("<vm guid>\<snapshot guid>\<resource guid>"), so exactly one
// segment may follow the owner.
inline std::string settings_owner_guid(const std::string &instance_id) {
  const std::string prefix = "Microsoft:";
  if (instance_id.compare(0, prefix.size(), prefix) != 0) return "";
  const std::string::size_type end = instance_id.find('\\', prefix.size());
  if (end == std::string::npos || instance_id.find('\\', end + 1) != std::string::npos) return "";
  const std::string owner = instance_id.substr(prefix.size(), end - prefix.size());
  return is_vm_guid(owner) ? to_lower(owner) : "";
}

// Msvm_VirtualSystemSettingData.VirtualSystemType of a checkpoint someone
// took. Hyper-V Replica keeps its recovery history as Snapshot:Recovery rows
// (and a replica its Snapshot:Replica ones), and a planned checkpoint is an
// import in flight; counting any of those would make every replicated VM look
// like it has forgotten checkpoints.
inline bool is_checkpoint_type(const std::string &type) { return type == "Microsoft:Hyper-V:Snapshot:Realized"; }

// Hyper-V's WMI provider only returns the virtual machines the caller is
// authorised for, and it hides the rest silently: an unelevated administrator
// or an ordinary user sees the host's own row and nothing else. So an empty
// list only means "no VMs" when something vouches for it. The health summary
// counters are not filtered that way: when they count VMs (`counted` > 0) the
// list is "not allowed to see them". But they only count VMs with a running
// worker process, so a count of none does not vouch for anything (every VM
// may be off or saved), no more than a count that could not be read
// (`counted` < 0) does. Then the caller's rights decide: `may_see_all` (an
// elevated administrator or a member of Hyper-V Administrators, which
// LocalSystem is) sees every VM, anyone else cannot tell an empty host from a
// hidden one. Returns the explanation to report, or an empty string when the
// list can be taken as it is.
inline std::string hidden_vms_reason(const std::size_t visible, const long long counted, const bool may_see_all) {
  const std::string fix = "run as an elevated administrator or a member of Hyper-V Administrators";
  if (visible > 0) return "";
  if (counted > 0) return "Hyper-V reports " + std::to_string(counted) + " virtual machine(s) on this host but none are visible to this account: " + fix;
  if (may_see_all) return "";
  return "Hyper-V shows no virtual machines to this account, and without the rights to see them all it cannot tell that there are none: " + fix;
}

// Msvm_VirtualSystemSettingData.VirtualSystemSubType is
// "Microsoft:Hyper-V:SubType:<n>"; the number is the VM generation. 0 when the
// value is missing or in another shape.
inline long long generation_of(const std::string &sub_type) {
  const std::string prefix = "Microsoft:Hyper-V:SubType:";
  if (sub_type.compare(0, prefix.size(), prefix) != 0) return 0;
  const std::string number = sub_type.substr(prefix.size());
  if (number.empty()) return 0;
  char *end = nullptr;
  const long long value = std::strtoll(number.c_str(), &end, 10);
  return (end != nullptr && *end == '\0') ? value : 0;
}

// ---------------------------------------------------------------------------
// Raw rows and the per-VM record they join into
// ---------------------------------------------------------------------------

// One Msvm_ComputerSystem row (a virtual machine; the host row is filtered
// out before it gets here).
struct raw_vm {
  std::string id;    // Name: the VM GUID, lower-cased
  std::string name;  // ElementName: the display name
  long long enabled_state = 0;
  long long health_state = 0;
  std::vector<long long> operational_status;
  long long uptime_ms = 0;                // OnTimeInMilliseconds (0 when off)
  long long last_state_change_epoch = 0;  // parsed TimeOfLastStateChange, 0 if unknown
  long long replication_mode = 0;
  long long replication_state = 0;
  long long replication_health = 0;
  long long process_id = 0;  // the vmwp.exe process (0 when off)
};

// One Msvm_HeartbeatComponent row.
struct raw_heartbeat {
  std::string vm_id;  // SystemName, lower-cased
  long long enabled_state = 0;
  std::vector<long long> operational_status;
};

// One Msvm_MemorySettingData row of a live configuration.
struct raw_memory_setting {
  std::string vm_id;         // derived from InstanceID
  long long startup_mb = 0;  // VirtualQuantity
  long long minimum_mb = 0;  // Reservation
  long long maximum_mb = 0;  // Limit
  bool dynamic = false;      // DynamicMemoryEnabled
};

// One Msvm_Memory row: memory currently backing a running VM.
struct raw_memory {
  std::string vm_id;    // SystemName, lower-cased
  long long bytes = 0;  // NumberOfBlocks * BlockSize
};

// One Msvm_ProcessorSettingData row of a live configuration.
struct raw_processor_setting {
  std::string vm_id;    // derived from InstanceID
  long long count = 0;  // VirtualQuantity
};

// One Msvm_Processor row: a virtual processor of a running VM.
struct raw_processor {
  std::string vm_id;  // SystemName, lower-cased
  long long load_percentage = 0;
};

// One Msvm_VirtualSystemSettingData row: the live configuration of a VM or
// one of its snapshots (VirtualSystemType tells which).
struct raw_settings {
  std::string vm_id;  // VirtualSystemIdentifier, lower-cased
  bool snapshot = false;
  std::string sub_type;          // VirtualSystemSubType (live configuration)
  std::string version;           // configuration version (live configuration)
  long long creation_epoch = 0;  // parsed CreationTime (snapshots)
};

struct vm_record {
  std::string id;
  std::string name;
  long long state_code = 0;
  long long health_code = 0;
  long long operational_status_code = 0;
  long long operation_code = 0;  // 0 when no operation is in progress
  long long uptime_seconds = 0;
  long long last_state_change_epoch = 0;
  long long process_id = 0;
  long long replication_mode = 0;
  long long replication_state = 0;
  long long replication_health = 0;

  // "disabled" when the heartbeat integration service is turned off in the VM
  // settings, "none" when the VM has no heartbeat component at all, otherwise
  // heartbeat_name() of the component's status.
  std::string heartbeat = "none";

  long long memory_assigned = 0;  // bytes backing the VM right now (0 when off)
  long long memory_startup = 0;   // bytes
  long long memory_minimum = 0;   // bytes (dynamic memory only, else 0)
  long long memory_maximum = 0;   // bytes (dynamic memory only, else 0)
  bool dynamic_memory = false;

  long long vcpus = 0;    // configured virtual processors
  double cpu_load = 0.0;  // mean LoadPercentage over the running processors

  long long generation = 0;
  std::string version;
  // How many VMs on this host carry this display name, this one included
  // (count_names). Hyper-V keys VMs by GUID and lets two share a name, which
  // breaks every tool that addresses VMs by name.
  long long name_count = 1;
  long long snapshots = 0;
  long long oldest_snapshot_epoch = 0;  // 0 when there are none

  std::string state() const { return vm_state_name(state_code); }
  std::string health() const { return vm_health_name(health_code); }
  std::string operational_status() const { return vm_operational_status_name(operational_status_code); }
  std::string operation() const { return operation_code == 0 ? "none" : vm_operation_name(operation_code); }
  std::string replication_mode_s() const { return replication_mode_name(replication_mode); }
  std::string replication_state_s() const { return replication_state_name(replication_state); }
  std::string replication_health_s() const { return replication_health_name(replication_health); }
  bool is_running() const { return state_code == 2; }
};

struct raw_rows {
  std::vector<raw_vm> vms;
  std::vector<raw_heartbeat> heartbeats;
  std::vector<raw_memory_setting> memory_settings;
  std::vector<raw_memory> memory;
  std::vector<raw_processor_setting> processor_settings;
  std::vector<raw_processor> processors;
  std::vector<raw_settings> settings;
};

// Fill vm_record::name_count: the number of records sharing each display
// name. Compared case-insensitively, the way Get-VM -Name and the Hyper-V
// Manager resolve a name; `fold` maps a name to its comparison key. This
// header stays platform-neutral, so the default folds ASCII only and the
// Windows caller passes a Unicode-aware fold (check_hyperv_vms.cpp). A VM
// whose name could not be read is not a duplicate of every other such VM.
template <typename Fold>
inline void count_names(std::vector<vm_record> &records, Fold fold) {
  std::map<std::string, long long> counts;
  for (const vm_record &record : records)
    if (!record.name.empty()) counts[fold(record.name)] += 1;
  for (vm_record &record : records) record.name_count = record.name.empty() ? 1 : counts[fold(record.name)];
}
inline void count_names(std::vector<vm_record> &records) { count_names(records, [](const std::string &name) { return to_lower(name); }); }

inline long long mb_to_bytes(const long long mb) { return mb * 1024LL * 1024LL; }

// Join the per-class rows into one record per VM, in the order the VM rows
// came in. Rows that reference a VM not in `rows.vms` are dropped.
// name_count is filled in too (count_names).
inline std::vector<vm_record> build_records(const raw_rows &rows) {
  std::vector<vm_record> out;
  std::map<std::string, std::size_t> index;
  for (const raw_vm &vm : rows.vms) {
    vm_record record;
    record.id = vm.id;
    record.name = vm.name;
    record.state_code = vm.enabled_state;
    record.health_code = vm.health_state;
    if (!vm.operational_status.empty()) record.operational_status_code = vm.operational_status[0];
    if (vm.operational_status.size() > 1) record.operation_code = vm.operational_status[1];
    record.uptime_seconds = vm.uptime_ms / 1000;
    record.last_state_change_epoch = vm.last_state_change_epoch;
    record.process_id = vm.process_id;
    record.replication_mode = vm.replication_mode;
    record.replication_state = vm.replication_state;
    record.replication_health = vm.replication_health;
    index[vm.id] = out.size();
    out.push_back(record);
  }
  const auto find = [&](const std::string &vm_id) -> vm_record * {
    const auto it = index.find(vm_id);
    return it == index.end() ? nullptr : &out[it->second];
  };

  for (const raw_heartbeat &hb : rows.heartbeats) {
    vm_record *record = find(hb.vm_id);
    if (record == nullptr) continue;
    if (hb.enabled_state == 3) {
      record->heartbeat = "disabled";
    } else if (!hb.operational_status.empty()) {
      record->heartbeat = heartbeat_name(hb.operational_status[0]);
    }
  }
  for (const raw_memory_setting &ms : rows.memory_settings) {
    vm_record *record = find(ms.vm_id);
    if (record == nullptr) continue;
    record->memory_startup = mb_to_bytes(ms.startup_mb);
    record->dynamic_memory = ms.dynamic;
    // Hyper-V fills Reservation and Limit for a static-memory VM too (both
    // are its fixed size), so they only mean minimum and maximum when
    // dynamic memory is on; otherwise they stay 0, as documented and as the
    // facts leave them out.
    if (ms.dynamic) {
      record->memory_minimum = mb_to_bytes(ms.minimum_mb);
      record->memory_maximum = mb_to_bytes(ms.maximum_mb);
    }
  }
  for (const raw_memory &m : rows.memory) {
    vm_record *record = find(m.vm_id);
    if (record == nullptr) continue;
    record->memory_assigned += m.bytes;
  }
  for (const raw_processor_setting &ps : rows.processor_settings) {
    vm_record *record = find(ps.vm_id);
    if (record == nullptr) continue;
    record->vcpus = ps.count;
  }
  std::map<std::string, std::pair<long long, long long> > load;  // vm id -> (sum, count)
  for (const raw_processor &p : rows.processors) {
    auto &entry = load[p.vm_id];
    entry.first += p.load_percentage;
    entry.second += 1;
  }
  for (const auto &entry : load) {
    vm_record *record = find(entry.first);
    if (record == nullptr || entry.second.second == 0) continue;
    record->cpu_load = static_cast<double>(entry.second.first) / static_cast<double>(entry.second.second);
  }
  for (const raw_settings &s : rows.settings) {
    vm_record *record = find(s.vm_id);
    if (record == nullptr) continue;
    if (s.snapshot) {
      record->snapshots += 1;
      if (s.creation_epoch > 0 && (record->oldest_snapshot_epoch == 0 || s.creation_epoch < record->oldest_snapshot_epoch)) {
        record->oldest_snapshot_epoch = s.creation_epoch;
      }
    } else {
      record->generation = generation_of(s.sub_type);
      record->version = s.version;
    }
  }
  count_names(out);
  return out;
}

// ---------------------------------------------------------------------------
// Host over-commitment
// ---------------------------------------------------------------------------

// What the running virtual machines take from the host, summed over the
// records build_records returned.
struct guest_usage {
  long long running = 0;          // running VMs
  long long vcpus = 0;            // virtual processors configured on the running VMs
  long long memory_assigned = 0;  // bytes currently backing the running VMs
};

inline guest_usage sum_running(const std::vector<vm_record> &records) {
  guest_usage usage;
  for (const vm_record &record : records) {
    if (!record.is_running()) continue;
    usage.running += 1;
    usage.vcpus += record.vcpus;
    usage.memory_assigned += record.memory_assigned;
  }
  return usage;
}

// The records whose VM has a worker process: running, and also paused (a
// paused VM keeps its worker and its memory). These are what the health
// summary counters count, so this is the number to hold them against.
inline long long with_worker_process(const std::vector<vm_record> &records) {
  long long active = 0;
  for (const vm_record &record : records)
    if (record.is_running() || record.process_id > 0) ++active;
  return active;
}

// Why a sum over `records` would under-report the guests, or an empty string
// when it can be trusted. Hyper-V's WMI provider only returns the VMs the
// caller is authorised for, so an account allowed some VMs gets a list that
// is merely short, and the sum over it reads as a nearly idle host. The health
// counters are not filtered, so `counted` (VMs with a worker process) fewer
// than the visible ones with a worker gives that away. Call after
// hidden_vms_reason, which handles a list with nothing visible at all.
inline std::string partial_vms_reason(const std::vector<vm_record> &records, const long long counted) {
  const long long visible = with_worker_process(records);
  if (visible >= counted) return "";
  return "only " + std::to_string(visible) + " of the " + std::to_string(counted) +
         " active virtual machine(s) Hyper-V reports are visible to this account: run as an elevated administrator or a member of Hyper-V "
         "Administrators";
}

// `part` as a percentage of `whole`, to one decimal; 0 when `whole` is not
// known (a counter that could not be read must not read as infinite).
inline double percent_of(const long long part, const long long whole) {
  if (whole <= 0) return 0.0;
  return std::round(static_cast<double>(part) * 1000.0 / static_cast<double>(whole)) / 10.0;
}

// ---------------------------------------------------------------------------
// Virtual switches
// ---------------------------------------------------------------------------

// Msvm_VirtualEthernetSwitch.EnabledState (the CIM table).
inline std::string switch_state_name(const long long state) {
  switch (state) {
    case 0:
      return "unknown";
    case 1:
      return "other";
    case 2:
      return "enabled";
    case 3:
      return "disabled";
    case 4:
      return "shutting_down";
    case 5:
      return "not_applicable";
    case 6:
      return "enabled_but_offline";
    case 7:
      return "in_test";
    case 8:
      return "deferred";
    case 9:
      return "quiesce";
    case 10:
      return "starting";
    default:
      return "state_" + std::to_string(state);
  }
}

// The GUID an Msvm_EthernetPortAllocationSettingData row hangs off: its
// InstanceID is "Microsoft:<owner>\<port>[\...]", where the owner is the
// switch for the switch's own ports and the VM for a VM's connection. Lower
// case; empty when the shape is anything else.
inline std::string port_owner_guid(const std::string &instance_id) {
  const std::string prefix = "Microsoft:";
  if (instance_id.compare(0, prefix.size(), prefix) != 0) return "";
  const std::string::size_type end = instance_id.find('\\', prefix.size());
  const std::string owner = instance_id.substr(prefix.size(), end == std::string::npos ? std::string::npos : end - prefix.size());
  return is_vm_guid(owner) ? to_lower(owner) : "";
}

// What a switch port's HostResource (the WMI path of what the port connects
// to) says about the switch: a physical adapter makes it external, the host
// (management OS) at least internal. Anything else (a VM's port points at
// the switch itself) says nothing.
enum class port_kind { other, external, internal };

inline port_kind classify_host_resource(const std::string &host_resource) {
  if (host_resource.find("Msvm_ExternalEthernetPort") != std::string::npos) return port_kind::external;
  if (host_resource.find("Msvm_WiFiPort") != std::string::npos) return port_kind::external;
  if (host_resource.find("Msvm_ComputerSystem") != std::string::npos) return port_kind::internal;
  return port_kind::other;
}

// One Msvm_VirtualEthernetSwitch row.
struct raw_switch {
  std::string id;    // Name: the switch GUID, lower-cased
  std::string name;  // ElementName
  long long enabled_state = 0;
  long long health_state = 0;
  std::vector<long long> operational_status;
};

// One Msvm_EthernetPortAllocationSettingData row that belongs to a switch.
struct raw_switch_port_setting {
  std::string switch_id;  // port_owner_guid(InstanceID)
  port_kind kind = port_kind::other;
};

// One Msvm_EthernetSwitchPort row.
struct raw_switch_port {
  std::string switch_id;  // SystemName, lower-cased
};

struct switch_rows {
  std::vector<raw_switch> switches;
  std::vector<raw_switch_port_setting> port_settings;
  std::vector<raw_switch_port> ports;
};

struct switch_record {
  std::string id;
  std::string name;
  long long state_code = 0;
  long long health_code = 0;
  long long operational_status_code = 0;
  // external (bound to a physical adapter, possibly shared with the host),
  // internal (host and VMs only) or private (VMs only), as Get-VMSwitch's
  // SwitchType reports it.
  std::string type = "private";
  bool management_os = false;  // the host has a virtual adapter on the switch
  long long ports = 0;         // ports on the switch: VM adapters plus the external/internal ones

  std::string state() const { return switch_state_name(state_code); }
  std::string health() const { return vm_health_name(health_code); }
  std::string operational_status() const { return vm_operational_status_name(operational_status_code); }
};

// Join the switch rows into one record per switch, in the order the switch
// rows came in. A switch with an external port is external whether or not
// the host shares the adapter; one with only the host's port is internal;
// one with neither is private.
inline std::vector<switch_record> build_switch_records(const switch_rows &rows) {
  std::vector<switch_record> out;
  std::map<std::string, std::size_t> index;
  for (const raw_switch &sw : rows.switches) {
    switch_record record;
    record.id = sw.id;
    record.name = sw.name;
    record.state_code = sw.enabled_state;
    record.health_code = sw.health_state;
    if (!sw.operational_status.empty()) record.operational_status_code = sw.operational_status[0];
    index[sw.id] = out.size();
    out.push_back(record);
  }
  std::map<std::string, std::pair<bool, bool> > kinds;  // switch id -> (external, internal)
  for (const raw_switch_port_setting &ps : rows.port_settings) {
    if (ps.kind == port_kind::external) kinds[ps.switch_id].first = true;
    if (ps.kind == port_kind::internal) kinds[ps.switch_id].second = true;
  }
  for (const auto &entry : kinds) {
    const auto it = index.find(entry.first);
    if (it == index.end()) continue;
    switch_record &record = out[it->second];
    record.management_os = entry.second.second;
    record.type = entry.second.first ? "external" : (entry.second.second ? "internal" : "private");
  }
  for (const raw_switch_port &port : rows.ports) {
    const auto it = index.find(port.switch_id);
    if (it != index.end()) out[it->second].ports += 1;
  }
  return out;
}

// ---------------------------------------------------------------------------
// Logical processor counters
// ---------------------------------------------------------------------------

// The PDH percentages come with six significant digits; one decimal is what
// a CPU figure is worth, in the detail line and in the perfdata alike.
inline double round1(const double value) { return std::round(value * 10.0) / 10.0; }

// One "Hyper-V Hypervisor Logical Processor" instance after sampling.
struct processor_sample {
  std::string processor;  // "Hv LP 3", or "total" for the synthetic aggregate
  double total_run_time = 0.0;
  double guest_run_time = 0.0;
  double hypervisor_run_time = 0.0;
  double idle_time = 0.0;
  double context_switches = 0.0;
};

// The PDH gather drops the _Total pseudo-instance, so the aggregate is built
// here: the mean of the percentages (what _Total reports for this object) and
// the sum of the context switches. Appended last; an empty input stays empty.
inline std::vector<processor_sample> with_total(std::vector<processor_sample> processors) {
  if (processors.empty()) return processors;
  processor_sample total;
  total.processor = "total";
  for (const processor_sample &p : processors) {
    total.total_run_time += p.total_run_time;
    total.guest_run_time += p.guest_run_time;
    total.hypervisor_run_time += p.hypervisor_run_time;
    total.idle_time += p.idle_time;
    total.context_switches += p.context_switches;
  }
  const double n = static_cast<double>(processors.size());
  total.total_run_time = round1(total.total_run_time / n);
  total.guest_run_time = round1(total.guest_run_time / n);
  total.hypervisor_run_time = round1(total.hypervisor_run_time / n);
  total.idle_time = round1(total.idle_time / n);
  processors.push_back(total);
  return processors;
}

}  // namespace check_hyperv_internal
}  // namespace check_hyperv
