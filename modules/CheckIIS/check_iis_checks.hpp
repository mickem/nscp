// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>

namespace check_iis {

// IIS health checks driven by the WAS/W3SVC/HTTP.sys performance counters,
// enriched (when the IIS WMI provider is installed) with configuration state
// from root\WebAdministration.
void check_iis_app_pools(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
void check_iis_sites(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
void check_iis_worker_processes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
void check_iis_request_queues(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_iis
