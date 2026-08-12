// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>

#include "mysql_client.hpp"

namespace check_mysql_command {

// Connectivity/health check. The session factory is injectable for unit
// testing; the module wires in mysql_session::make_session_factory().
void check_with(const mysql_client::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                PB::Commands::QueryResponseMessage::Response *response, const mysql_client::session_factory &factory);

}  // namespace check_mysql_command
