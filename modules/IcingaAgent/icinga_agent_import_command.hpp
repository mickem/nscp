// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// `nscp icinga-agent import`: the command line around icinga_agent_import.hpp
// (who the agent is and who it reports to) and icinga_agent_pki.hpp (whether
// its identity can be used, and storing it).

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace icinga_agent {

// Always true: the command was handled, and `response` says how it went.
bool run_import_command(nscapi::core_wrapper *core, unsigned int plugin_id, const std::string &certificate_directory,
                        const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);

}  // namespace icinga_agent
