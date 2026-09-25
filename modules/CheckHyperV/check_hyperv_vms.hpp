// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>
#include <win/wmi/wmi_query.hpp>

#include "check_hyperv_internal.hpp"

namespace check_hyperv {

// Per-VM check driven by the root\virtualization\v2 WMI namespace.
void check_hyperv_vms(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

// Fetch the rows behind check_hyperv_vms (the caller holds the COM scope).
// Throws wmi_impl::wmi_exception; hyperv_unavailable() tells "Hyper-V is not
// there" apart from a real error.
check_hyperv_internal::raw_rows fetch_vm_rows();

// Why Hyper-V cannot be queried at all, worded to finish a sentence ("the
// Hyper-V role is not installed on this host ..."), or an empty string when
// `e` is an ordinary failure to be reported as it is.
std::string hyperv_unavailable(const wmi_impl::wmi_exception &e);

// Why `visible` VMs (what fetch_vm_rows found) is not the whole story, or an
// empty string when it is: cross-checks against the health summary counters,
// which WMI's per-caller authorisation does not filter.
std::string vms_hidden_from_caller(std::size_t visible);

}  // namespace check_hyperv
