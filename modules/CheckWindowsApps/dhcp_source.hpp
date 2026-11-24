// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <string>
#include <vector>

#include "dhcp_filter.hpp"

// The Windows readers behind the DHCP Server checks (dhcp_source_win.cpp). Both
// read the DHCP server on this host and have the shape of the fetchers in
// check_dhcp.hpp.
namespace dhcp_source {

// Every IPv4 scope of the local DHCP server with its address statistics:
// DhcpEnumSubnets for the list, DhcpGetSubnetInfo for each scope's mask, name
// and state, DhcpGetMibInfo for the in-use/free/offered counts.
bool fetch_scopes(std::vector<dhcp_filter::scope_obj_ptr> &out, std::string &error);

// The "DHCP Server" performance counters, sampled twice a second apart so the
// per-second rates carry real values.
bool fetch_counters(std::map<std::string, double> &out, std::string &error);

}  // namespace dhcp_source
