// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "hyperv_facts.hpp"

#include <map>
#include <nscapi/nscapi_facts_helper.hpp>

namespace hyperv_facts {

using check_hyperv::check_hyperv_internal::vm_record;

const char *const set_hyperv = "hyperv";
const char *const key_vms = "vms";
const char *const id_vms = "hyperv.vms";

std::vector<std::string> record_ids(const std::vector<vm_record> &vms) {
  std::map<std::string, int> seen;
  for (const vm_record &vm : vms) seen[vm.name] += 1;
  std::vector<std::string> ids;
  ids.reserve(vms.size());
  for (const vm_record &vm : vms) {
    // A VM without a name (WMI gave none) still has its GUID, which is what
    // the check shows for it too.
    if (vm.name.empty()) {
      ids.push_back(vm.id);
    } else if (seen[vm.name] > 1) {
      ids.push_back(vm.name + " (" + vm.id + ")");
    } else {
      ids.push_back(vm.name);
    }
  }
  return ids;
}

void publish(const std::vector<vm_record> &vms, const std::time_t taken_at, nscapi::facts::response &out) {
  // The list is written even when it is empty: a Hyper-V host with no VM has
  // told us something, and an absent list would read as "not collected".
  nscapi::facts::record_list list = out.set(set_hyperv).list(key_vms);
  const std::vector<std::string> ids = record_ids(vms);
  for (std::size_t i = 0; i < vms.size(); ++i) {
    const vm_record &vm = vms[i];
    nscapi::facts::section record = list.record(ids[i]);
    record.value("name", vm.name).value("vm_id", vm.id).value("version", vm.version);
    // Zero is "not read" (a VM whose settings rows did not join, a VM of a
    // Hyper-V version without a generation), and the builder writes a number
    // as it is given, so the guards are here.
    if (vm.generation > 0) record.value("generation", vm.generation);
    if (vm.vcpus > 0) record.value("vcpus", vm.vcpus);
    if (vm.memory_startup > 0) record.value("memory_startup_bytes", vm.memory_startup);
    record.value("dynamic_memory", vm.dynamic_memory);
    if (vm.dynamic_memory) {
      if (vm.memory_minimum > 0) record.value("memory_minimum_bytes", vm.memory_minimum);
      if (vm.memory_maximum > 0) record.value("memory_maximum_bytes", vm.memory_maximum);
    }
    record.value("checkpoints", vm.snapshots);
    record.value("replication_mode", vm.replication_mode_s());
  }
  out.gathered(set_hyperv, taken_at);
}

}  // namespace hyperv_facts
