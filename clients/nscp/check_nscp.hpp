// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/command.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

#include "../modules/NSCPClient/nscp_client.hpp"

class check_nscp {
 private:
  client::configuration client_;

 public:
  check_nscp();
  void query(const PB::Commands::QueryRequestMessage &request, PB::Commands::QueryResponseMessage &response);
};