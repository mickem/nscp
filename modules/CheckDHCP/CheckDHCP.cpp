// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckDHCP.h"

#include "check_dhcp.hpp"
#include "dhcp_source.hpp"

// No state and no threads: each check reads the server when it runs, so a
// settings reload has nothing to restart.
bool CheckDHCP::loadModuleEx(std::string, NSCAPI::moduleLoadMode) { return true; }

bool CheckDHCP::unloadModule() { return true; }

void CheckDHCP::check_dhcp_scopes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_dhcp::check_dhcp_scopes(request, response, dhcp_source::fetch_scopes);
}

void CheckDHCP::check_dhcp_server(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_dhcp::check_dhcp_server(request, response, dhcp_source::fetch_counters);
}
