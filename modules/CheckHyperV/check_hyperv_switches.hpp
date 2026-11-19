// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>

namespace check_hyperv {

// Per-switch check driven by Msvm_VirtualEthernetSwitch and its ports in the
// root\virtualization\v2 WMI namespace.
void check_hyperv_switches(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_hyperv
