// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <functional>
#include <map>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "dhcp_filter.hpp"

// The CheckDHCP checks, platform-neutral: the data comes in through a fetcher
// so the module wires in the Windows readers (dhcp_source_win.cpp) and the
// unit test wires in canned data, and both run the same option parsing,
// thresholds and rendering.
namespace check_dhcp {

// Fills `out` with one record per IPv4 scope. Returns false with `error` set
// when the server cannot be read; the check then reports UNKNOWN with it.
typedef std::function<bool(std::vector<dhcp_filter::scope_obj_ptr> &out, std::string &error)> scope_fetcher;

// Fills `out` with the "DHCP Server" counters (counter name -> value per
// second or queue length). Returns false with `error` set when the counter
// set cannot be read.
typedef std::function<bool(std::map<std::string, double> &out, std::string &error)> counter_fetcher;

void check_dhcp_scopes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                       const scope_fetcher &fetch);

void check_dhcp_server(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                       const counter_fetcher &fetch);

}  // namespace check_dhcp
