// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <vector>
#include <win/wmi/wmi_query.hpp>

#include "check_hyperv_internal.hpp"

namespace check_hyperv {

// Per-VM check driven by the root\virtualization\v2 WMI namespace.
void check_hyperv_vms(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

// Fetch the rows behind check_hyperv_vms (the caller holds the COM scope).
// Throws wmi_impl::wmi_exception; is_hyperv_missing() tells the "role not
// installed" case apart from a real error.
check_hyperv_internal::raw_rows fetch_vm_rows();
bool is_hyperv_missing(const wmi_impl::wmi_exception &e);

}  // namespace check_hyperv
