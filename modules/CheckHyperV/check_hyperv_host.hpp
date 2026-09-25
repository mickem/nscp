// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace check_hyperv {

// Host-level checks driven by the hypervisor performance counters.
void check_hyperv_host(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
void check_hyperv_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

// The host counters check_hyperv_host reads, keyed by counter name, for the
// metrics collector. Throws PDH::pdh_exception when this is not a Hyper-V host.
std::map<std::string, double> fetch_host_counters();

// The virtual machines the health summary in `host` (as fetch_host_counters
// returns it) counts: the healthy ones plus the critical ones.
long long counted_vms(const std::map<std::string, double> &host);

}  // namespace check_hyperv
