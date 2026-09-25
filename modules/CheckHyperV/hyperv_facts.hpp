// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "check_hyperv_internal.hpp"

namespace nscapi {
namespace facts {
class response;
}
}  // namespace nscapi

// The `hyperv` fact set: which virtual machines this host carries and how
// each one is configured.
//
// The inventory, not the monitoring: a VM's generation, its configured
// memory and processors and whether it is replicated are here; its power
// state, heartbeat, load and assigned memory are not. Those move from one
// round to the next, and a value that moves every round would bump the
// document's revision every round for no new information. check_hyperv_vms
// is where they live.
//
// The records are built from the same vm_record check_hyperv_vms reads, so
// the two cannot disagree about what a VM is called: the record id is the
// `vm` keyword of the check.
namespace hyperv_facts {

// The fact set this module produces, and the one list in it. The enableable
// id is the dotted path, `hyperv.vms`: that is the settings key.
extern const char *const set_hyperv;
extern const char *const key_vms;
extern const char *const id_vms;

// The record id of each VM, in the order given: its name, which is what
// check_hyperv_vms calls `vm`. Hyper-V does not require names to be unique,
// and an id has to be unique in its list, so a name shared by several VMs
// has the VM's GUID appended on every one of them.
std::vector<std::string> record_ids(const std::vector<check_hyperv::check_hyperv_internal::vm_record> &vms);

// Add the `hyperv` set, with every VM in `vms` as a record of its `vms` list,
// to `out`. `taken_at` stamps when the values were read.
void publish(const std::vector<check_hyperv::check_hyperv_internal::vm_record> &vms, std::time_t taken_at, nscapi::facts::response &out);

}  // namespace hyperv_facts
